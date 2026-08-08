#pragma once

#include <Arduino.h>

// Result callback for a single register read/write.
// success=false covers: not connected, request timeout, or a Modbus exception response.
typedef void (*ModbusResultCallback)(bool success, uint16_t value);

// Result callback for a 32-bit value spanning two consecutive registers.
typedef void (*Modbus32ResultCallback)(bool success, uint32_t value);

void modbusInit();
void modbusLoop();

void modbusReadInputReg(uint16_t reg, ModbusResultCallback cb);
void modbusWriteHoldingReg(uint16_t reg, uint16_t value, ModbusResultCallback cb = nullptr);

// Reads reg and reg+1 as a single big-endian 32-bit value (reg = high word,
// reg+1 = low word), matching the Heidelberg energy counter register pairs.
void modbusReadInputReg32(uint16_t reg, Modbus32ResultCallback cb);
