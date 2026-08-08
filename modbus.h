#pragma once

#include <Arduino.h>

// Result callback for a single register read/write.
// success=false covers: not connected, request timeout, or a Modbus exception response.
typedef void (*ModbusResultCallback)(bool success, uint16_t value);

void modbusInit();
void modbusLoop();

void modbusReadInputReg(uint16_t reg, ModbusResultCallback cb);
void modbusWriteHoldingReg(uint16_t reg, uint16_t value, ModbusResultCallback cb = nullptr);
