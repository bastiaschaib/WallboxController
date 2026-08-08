#include <SoftwareSerial.h>
#include <ModbusRTU.h>
#include "modbus.h"
#include "config.h"

// Thin transport wrapper: owns the RS485 serial link and the RTU master,
// and turns the library's transaction-ID-keyed callback API into a simple
// per-call ModbusResultCallback, so wallbox.cpp never has to touch
// ModbusRTU directly.

static SoftwareSerial rs485(MODBUS_RTU_RX_PIN, MODBUS_RTU_TX_PIN);
static ModbusRTU mb;

namespace {
  constexpr int MAX_PENDING = 4;

  struct PendingRequest {
    uint16_t transactionId = 0;
    ModbusResultCallback cb = nullptr;
    uint16_t value = 0;
    bool active = false;
  };

  PendingRequest pending[MAX_PENDING];

  PendingRequest* allocSlot() {
    for (auto &p : pending) {
      if (!p.active) return &p;
    }
    return nullptr;
  }

  PendingRequest* findSlot(uint16_t transactionId) {
    for (auto &p : pending) {
      if (p.active && p.transactionId == transactionId) return &p;
    }
    return nullptr;
  }

  bool onTransactionResult(Modbus::ResultCode event, uint16_t transactionId, void*) {
    PendingRequest* p = findSlot(transactionId);
    if (!p) return true; // stale/unknown transaction, ignore

    ModbusResultCallback cb = p->cb;
    uint16_t value = p->value;
    p->active = false;

    if (cb) cb(event == Modbus::EX_SUCCESS, value);
    return true;
  }
}

void modbusInit() {
  rs485.begin(MODBUS_BAUD, SWSERIAL_8E1);
  mb.begin(&rs485, MODBUS_RTU_DE_RE_PIN);
  mb.master();
}

void modbusLoop() {
  mb.task();
}

void modbusReadInputReg(uint16_t reg, ModbusResultCallback cb) {
  PendingRequest* p = allocSlot();
  if (!p) {
    if (cb) cb(false, 0); // request table full, drop this poll cycle
    return;
  }

  p->active = true;
  p->cb = cb;
  uint16_t trans = mb.readIreg(MODBUS_SLAVE_ID, reg, &p->value, 1, onTransactionResult);
  if (trans == 0) {
    p->active = false;
    if (cb) cb(false, 0);
    return;
  }
  p->transactionId = trans;
}

void modbusWriteHoldingReg(uint16_t reg, uint16_t value, ModbusResultCallback cb) {
  PendingRequest* p = allocSlot();
  if (!p) {
    if (cb) cb(false, 0);
    return;
  }

  p->active = true;
  p->cb = cb;
  p->value = value;
  uint16_t trans = mb.writeHreg(MODBUS_SLAVE_ID, reg, value, onTransactionResult);
  if (trans == 0) {
    p->active = false;
    if (cb) cb(false, 0);
    return;
  }
  p->transactionId = trans;
}
