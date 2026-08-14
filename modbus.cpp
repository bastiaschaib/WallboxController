#include <new>
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

// Set to 1 to log every raw byte crossing the wire (with a millis() timestamp)
// as seen directly at the SoftwareSerial boundary, before the Modbus library
// gets a chance to interpret/reject it. Diagnostic-only: remove once the link
// is confirmed reliable, since it prints on every single byte.
#define MODBUS_WIRE_SNIFFER 0

#if MODBUS_WIRE_SNIFFER
// Transparent passthrough Stream so mb.begin() can use it exactly like the
// SoftwareSerial it wraps, while logging every byte read from / written to it.
class ModbusWireSniffer : public Stream {
  public:
    explicit ModbusWireSniffer(SoftwareSerial &port) : _port(port) {}

    int available() override { return _port.available(); }
    int peek() override { return _port.peek(); }

    int read() override {
      int c = _port.read();
      if (c >= 0) logByte("rx", (uint8_t)c);
      return c;
    }

    size_t write(uint8_t b) override {
      logByte("tx", b);
      return _port.write(b);
    }

    void flush() override { _port.flush(); }
    uint32_t baudRate() { return _port.baudRate(); }

  private:
    SoftwareSerial &_port;

    static void logByte(const char* dir, uint8_t b) {
      Serial.print("[wire] ");
      Serial.print(dir);
      Serial.print(' ');
      if (b < 0x10) Serial.print('0');
      Serial.print(b, HEX);
      Serial.print(" @");
      Serial.println(millis());
    }
};

static ModbusWireSniffer rs485Sniffer(rs485);
#endif

namespace {
  // ModbusRTU only ever has one transaction in flight (enforced both by the
  // library itself and by wallbox.cpp's polling chain), and its callback
  // always reports transaction id 0 regardless of what send()/readIreg()
  // returned - RTU has no real per-request id, unlike the TCP variant this
  // API was designed around. So rather than matching on that id, just track
  // the one callback that's currently outstanding and invoke it directly.
  ModbusResultCallback pendingCb = nullptr;
  uint16_t pendingValue = 0;

  Modbus32ResultCallback pendingCb32 = nullptr;
  uint16_t pendingValue32[2] = {0, 0};

  bool onTransactionResult(Modbus::ResultCode event, uint16_t, void*) {
    ModbusResultCallback cb = pendingCb;
    uint16_t value = pendingValue;
    pendingCb = nullptr;

    if (cb) cb(event == Modbus::EX_SUCCESS, value);
    return true;
  }

  bool onTransactionResult32(Modbus::ResultCode event, uint16_t, void*) {
    Modbus32ResultCallback cb = pendingCb32;
    uint32_t value = ((uint32_t)pendingValue32[0] << 16) | pendingValue32[1];
    pendingCb32 = nullptr;

    if (cb) cb(event == Modbus::EX_SUCCESS, value);
    return true;
  }
}

void modbusInit() {
  rs485.begin(MODBUS_BAUD, SWSERIAL_8E1);
#if MODBUS_WIRE_SNIFFER
  mb.begin(&rs485Sniffer, MODBUS_RTU_DE_RE_PIN);
#else
  mb.begin(&rs485, MODBUS_RTU_DE_RE_PIN);
#endif
  mb.master();
}

void modbusLoop() {
  mb.task();
}

void modbusReset() {
  while (rs485.available()) rs485.read(); // drop stray bytes so a fresh mb doesn't misread them as a reply

  // ModbusRTU's transaction state (in-flight slave id, receive length, timeout
  // timestamp) is private and has no public reset, so placement-new the
  // master back to a clean-constructed state rather than reaching in.
  mb.~ModbusRTU();
  new (&mb) ModbusRTU();
#if MODBUS_WIRE_SNIFFER
  mb.begin(&rs485Sniffer, MODBUS_RTU_DE_RE_PIN);
#else
  mb.begin(&rs485, MODBUS_RTU_DE_RE_PIN);
#endif
  mb.master();

  // Whatever request was in flight when the link got stuck will never get
  // its callback now; cancel our own bookkeeping for it too.
  pendingCb = nullptr;
  pendingCb32 = nullptr;
}

void modbusReadInputReg(uint16_t reg, ModbusResultCallback cb) {
  // If the send doesn't actually go out (link busy with something else in
  // flight), restore whatever was pending before so we don't clobber the
  // callback for a real in-flight transaction.
  ModbusResultCallback previousCb = pendingCb;
  uint16_t previousValue = pendingValue;

  pendingCb = cb;
  uint16_t trans = mb.readIreg(MODBUS_SLAVE_ID, reg, &pendingValue, 1, onTransactionResult);
  if (trans == 0) {
    pendingCb = previousCb;
    pendingValue = previousValue;
    if (cb) cb(false, 0);
  }
}

void modbusReadInputReg32(uint16_t reg, Modbus32ResultCallback cb) {
  Modbus32ResultCallback previousCb = pendingCb32;
  uint16_t previousValue[2] = {pendingValue32[0], pendingValue32[1]};

  pendingCb32 = cb;
  uint16_t trans = mb.readIreg(MODBUS_SLAVE_ID, reg, pendingValue32, 2, onTransactionResult32);
  if (trans == 0) {
    pendingCb32 = previousCb;
    pendingValue32[0] = previousValue[0];
    pendingValue32[1] = previousValue[1];
    if (cb) cb(false, 0);
  }
}

void modbusWriteHoldingReg(uint16_t reg, uint16_t value, ModbusResultCallback cb) {
  ModbusResultCallback previousCb = pendingCb;
  uint16_t previousValue = pendingValue;

  pendingCb = cb;
  pendingValue = value;
  uint16_t trans = mb.writeHreg(MODBUS_SLAVE_ID, reg, value, onTransactionResult);
  if (trans == 0) {
    pendingCb = previousCb;
    pendingValue = previousValue;
    if (cb) cb(false, 0);
  }
}
