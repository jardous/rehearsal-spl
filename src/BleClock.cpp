// SPDX-License-Identifier: GPL-3.0-or-later
#include "BleClock.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_timer.h>
#include <atomic>
#include <cstring>

namespace bleClock {
namespace {
constexpr uint16_t noConnection = BLE_HS_CONN_HANDLE_NONE;
std::atomic<uint16_t> connection{noConnection};
std::atomic<uint32_t> generation{0};
std::atomic<ClockLink> link{ClockLink::Starting};
std::atomic<bool> servicesChanged{false}, syncRequested{false}, forgetRequested{false};
NimBLEServer* server = nullptr;
QueueHandle_t updates = nullptr;
portMUX_TYPE clockLock = portMUX_INITIALIZER_UNLOCKED;
ClockTime clockTime;
struct Update {
  uint8_t bytes[10];
  uint64_t receivedUs;
  uint32_t generation;
};

void queueTime(const uint8_t* bytes, size_t length, uint32_t session) {
  if (length != 10 || !bytes || !updates) return;
  Update update;
  memcpy(update.bytes, bytes, sizeof(update.bytes));
  update.receivedUs = esp_timer_get_time();
  update.generation = session;
  xQueueOverwrite(updates, &update);
}
void timeNotification(NimBLERemoteCharacteristic* characteristic, uint8_t* data,
                      size_t length, bool) {
  if (characteristic->getClient()->getConnHandle() == connection.load())
    queueTime(data, length, generation.load());
}
void serviceNotification(NimBLERemoteCharacteristic*, uint8_t*, size_t length, bool) {
  if (length == 4) servicesChanged = true; // worker reconnects with a fresh database
}
class Callbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& info) override {
    ++generation;
    connection = info.getConnHandle();
    link = ClockLink::Pairing;
    s->updateConnParams(info.getConnHandle(), 24, 36, 0, 400); // 30–45 ms, 4 s
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    connection = noConnection;
    ++generation;
    link = ClockLink::Advertising;
    // NimBLE restarts advertising; the local clock deliberately survives.
  }
} callbacks;

bool advertise() {
  auto* advertising = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData data;
  // Name and CTS solicitation in the primary packet (not a fictitious hosted
  // CTS server). AD type 0x14 = list of solicited 16-bit service UUIDs.
  const uint8_t solicitation[] = {3, 0x14, 0x05, 0x18};
  if (!data.setFlags(0x06) || !data.setName("Rehearsal SPL")
      || !data.addData(solicitation, sizeof(solicitation))
      || !advertising->setAdvertisementData(data)) return false;
  advertising->setMinInterval(244); // Apple's recommended 152.5 ms
  advertising->setMaxInterval(244);
  return advertising->start();
}

void worker(void*) {
  if (!NimBLEDevice::init("Rehearsal SPL")) {
    link = ClockLink::Error;
    vTaskDelete(nullptr);
    return;
  }
  NimBLEDevice::setSecurityAuth(true, false, true); // bond, Just Works, secure connections
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  server = NimBLEDevice::createServer();
  server->setCallbacks(&callbacks, false);
  server->advertiseOnDisconnect(true);
  // Expose only read-only standard device information.
  auto* dis = server->createService(NimBLEUUID(uint16_t(0x180a)));
  dis->createCharacteristic(NimBLEUUID(uint16_t(0x2a29)), NIMBLE_PROPERTY::READ)
    ->setValue("Rehearsal SPL");
  dis->createCharacteristic(NimBLEUUID(uint16_t(0x2a24)), NIMBLE_PROPERTY::READ)
    ->setValue("MatrixPortal S3");
  link = server->start() && advertise() ? ClockLink::Advertising : ClockLink::Error;
  uint32_t session = 0, attemptedAt = 0, readAt = 0;
  NimBLEClient* client = nullptr;
  NimBLERemoteCharacteristic* currentTime = nullptr;
  bool watchedServices = false, subscribed = false;
  for (;;) {
    if (forgetRequested.exchange(false)) {
      const uint16_t handle = connection.load();
      if (handle != noConnection) server->disconnect(handle);
      NimBLEDevice::deleteAllBonds();
    }
    const uint32_t activeSession = generation.load();
    const uint16_t handle = connection.load();
    if (session != activeSession) {
      session = activeSession;
      client = nullptr;
      currentTime = nullptr;
      subscribed = watchedServices = false;
      servicesChanged = false;
      attemptedAt = millis() - 3000;
      readAt = millis() - 60000;
    }
    const uint32_t now = millis();
    if (handle != noConnection && servicesChanged.exchange(false)) {
      // Do not free remote attributes while the host can dispatch notifications
      // through them. Reconnect and let getClient() clear the old database before
      // binding the next connection. The last good time continues in holdover.
      server->disconnect(handle);
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    if (handle != noConnection && uint32_t(now - attemptedAt) >= 3000) {
      attemptedAt = now;
      if (!client) client = server->getClient(handle); // use the INBOUND phone connection
      if (client && client->getConnHandle() == handle && generation.load() == session) {
        if (!client->getConnInfo().isEncrypted()) {
          link = ClockLink::Pairing;
          // Keep pairing asynchronous: an inbound-link disconnect need not wake
          // NimBLEClient's synchronous security waiter. Generation checks below
          // handle rejection/disconnection without stranding this worker.
          client->secureConnection(true);
        }
        if (generation.load() == session && client->getConnInfo().isEncrypted()) {
          if (!watchedServices) {
            auto* gatt = client->getService(NimBLEUUID(uint16_t(0x1801)));
            auto* changed = gatt ? gatt->getCharacteristic(NimBLEUUID(uint16_t(0x2a05))) : nullptr;
            watchedServices = changed && changed->canIndicate()
              && changed->subscribe(false, serviceNotification);
          }
          if (!currentTime) {
            link = ClockLink::Discovering;
            auto* cts = client->getService(NimBLEUUID(uint16_t(0x1805)));
            currentTime = cts ? cts->getCharacteristic(NimBLEUUID(uint16_t(0x2a2b))) : nullptr;
            readAt = now - 60000; // initial read immediately, also after delayed discovery
          }
          if (currentTime && currentTime->canRead()) {
            if (!subscribed && currentTime->canNotify())
              subscribed = currentTime->subscribe(true, timeNotification);
            const bool requested = syncRequested.exchange(false);
            if (uint32_t(now - readAt) >= 60000 || requested) {
              readAt = now;
              const auto value = currentTime->readValue();
              if (generation.load() == session) {
                queueTime(value.data(), value.size(), session);
                if (value.size() != 10) link = ClockLink::Error;
              }
            }
          } else if (generation.load() == session) link = ClockLink::NoCTS;
        }
      }
    }
    Update update;
    if (xQueueReceive(updates, &update, 0) == pdTRUE && update.generation == generation.load()
        && connection.load() != noConnection) {
      ClockTime next;
      if (next.sync(update.bytes, sizeof(update.bytes), update.receivedUs)) {
        portENTER_CRITICAL(&clockLock);
        clockTime = next;
        portEXIT_CRITICAL(&clockLock);
        link = ClockLink::Ready;
      } else link = ClockLink::Error; // invalid data never destroys the last good time
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
} // namespace

bool begin() {
  updates = xQueueCreate(1, sizeof(Update));
  if (updates && xTaskCreatePinnedToCore(worker, "ble-clock", 6144, nullptr, 1, nullptr, 1) == pdPASS)
    return true;
  if (updates) { vQueueDelete(updates); updates = nullptr; }
  link = ClockLink::Error;
  return false;
}
ClockSnapshot snapshot() {
  portENTER_CRITICAL(&clockLock);
  const ClockTime copy = clockTime;
  portEXIT_CRITICAL(&clockLock);
  auto result = copy.snapshot(esp_timer_get_time());
  result.link = link.load();
  result.connected = connection.load() != noConnection;
  return result;
}
void requestSync() { syncRequested = true; }
void forgetPhone() { forgetRequested = true; }
} // namespace bleClock
