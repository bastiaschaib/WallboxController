#!/usr/bin/env python3
"""Modbus RTU slave simulator for the Heidelberg Energy Control wallbox.

Stands in for the real wallbox on the RS485 bus so the ESP8266 firmware
(modbus.cpp / wallbox.cpp) can be tested without it. Implements only the
registers that firmware actually touches:

  - input register   5      (FC04) charging state, 2-11 (7 = C2 = charging)
  - input register  14      (FC04) power, VA
  - input registers 15+16   (FC04) energy since power-on, VAh (32-bit, high word first)
  - input registers 17+18   (FC04) energy since installation, VAh (32-bit, high word first)
  - holding register 261    (FC03/FC06) max current, 0.1A steps (0 or 60-160)

Run it, then use the interactive console to change what the simulated
wallbox reports while the ESP8266 polls it. Every write the firmware makes
to the current-setpoint register is printed as it arrives.

Usage:
  python3 modbus_sim.py /dev/ttyUSB0
  python3 modbus_sim.py /dev/ttyUSB0 --baud 19200 --slave-id 1

Console commands:
  state <2-11>        set charging state (2=A1 3=A2 4=B1 5=B2 6=C1 7=C2 8=derating 9=E 10=F 11=err)
  power <VA>           set reported power
  energy_session <VAh> set energy-since-power-on counter
  energy_total <VAh>   set energy-since-installation counter
  show                 print current register values
  quit
"""

import argparse
import threading
from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from pymodbus.server import StartSerialServer

REG_CHARGING_STATE = 5
REG_POWER = 14
REG_ENERGY_POWERON = 15  # + 16
REG_ENERGY_TOTAL = 17    # + 18
REG_MAX_CURRENT = 261


def set_energy(ir_block, reg, volt_amp_hours):
    ir_block.setValues(reg, [(volt_amp_hours >> 16) & 0xFFFF, volt_amp_hours & 0xFFFF])


def get_energy(ir_block, reg):
    hi, lo = ir_block.getValues(reg, 2)
    return (hi << 16) | lo

STATE_NAMES = {
    2: "A1 (not connected)",
    3: "A2 (not connected, no permission)",
    4: "B1 (connected)",
    5: "B2 (connected, no permission)",
    6: "C1 (charging requested, wallbox not allowing)",
    7: "C2 (charging)",
    8: "derating",
    9: "E (error, VDE0100)",
    10: "F (wallbox error)",
    11: "error (RCD/EVSE)",
}


class LoggingHoldingBlock(ModbusSequentialDataBlock):
    def setValues(self, address, values):
        super().setValues(address, values)
        if address == REG_MAX_CURRENT:
            deciamps = values[0]
            amps = deciamps / 10
            print(f"[wallbox->sim] holding reg {REG_MAX_CURRENT} (max current) <- {deciamps} (={amps}A)")


def build_context(slave_id):
    ir_size = REG_ENERGY_TOTAL + 10
    hr_size = REG_MAX_CURRENT + 10
    ir_block = ModbusSequentialDataBlock(0, [0] * ir_size)
    hr_block = LoggingHoldingBlock(0, [0] * hr_size)
    ir_block.setValues(REG_CHARGING_STATE, [4])  # start "plugged in, no permission" like a freshly connected car
    slave = ModbusSlaveContext(di=None, co=None, ir=ir_block, hr=hr_block, zero_mode=True)
    return ModbusServerContext(slaves={slave_id: slave}, single=False), ir_block, hr_block


def console(ir_block, hr_block):
    print("Simulator running. Commands: state <2-11> | power <VA> | energy_session <VAh> | "
          "energy_total <VAh> | show | quit")
    while True:
        try:
            line = input("> ").strip().split()
        except EOFError:
            # stdin closed (e.g. piped input ran out) - keep the server
            # running, just stop reading commands.
            return
        except KeyboardInterrupt:
            break
        if not line:
            continue
        cmd = line[0].lower()
        if cmd == "quit":
            break
        elif cmd == "state" and len(line) == 2:
            try:
                val = int(line[1])
            except ValueError:
                print("state must be an integer 2-11")
                continue
            if val not in STATE_NAMES:
                print(f"unknown state {val}, expected one of {sorted(STATE_NAMES)}")
                continue
            ir_block.setValues(REG_CHARGING_STATE, [val])
            print(f"charging state -> {val} ({STATE_NAMES[val]})")
        elif cmd == "power" and len(line) == 2:
            try:
                val = int(line[1])
            except ValueError:
                print("power must be an integer (VA)")
                continue
            ir_block.setValues(REG_POWER, [val])
            print(f"power -> {val} VA")
        elif cmd == "energy_session" and len(line) == 2:
            try:
                val = int(line[1])
            except ValueError:
                print("energy_session must be an integer (VAh)")
                continue
            set_energy(ir_block, REG_ENERGY_POWERON, val)
            print(f"energy since power-on -> {val} VAh")
        elif cmd == "energy_total" and len(line) == 2:
            try:
                val = int(line[1])
            except ValueError:
                print("energy_total must be an integer (VAh)")
                continue
            set_energy(ir_block, REG_ENERGY_TOTAL, val)
            print(f"energy since installation -> {val} VAh")
        elif cmd == "show":
            state = ir_block.getValues(REG_CHARGING_STATE, 1)[0]
            power = ir_block.getValues(REG_POWER, 1)[0]
            current = hr_block.getValues(REG_MAX_CURRENT, 1)[0]
            energy_session = get_energy(ir_block, REG_ENERGY_POWERON)
            energy_total = get_energy(ir_block, REG_ENERGY_TOTAL)
            print(f"state={state} ({STATE_NAMES.get(state, '?')})  power={power}VA  "
                  f"max_current_setpoint={current} ({current/10}A)  "
                  f"energy_session={energy_session}VAh  energy_total={energy_total}VAh")
        else:
            print("commands: state <2-11> | power <VA> | energy_session <VAh> | energy_total <VAh> | show | quit")
    print("Shutting down...")
    import os
    os._exit(0)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port", help="serial device, e.g. /dev/ttyUSB0")
    parser.add_argument("--baud", type=int, default=19200)
    parser.add_argument("--slave-id", type=int, default=1)
    args = parser.parse_args()

    context, ir_block, hr_block = build_context(args.slave_id)

    t = threading.Thread(target=console, args=(ir_block, hr_block), daemon=True)
    t.start()

    print(f"Listening as Modbus RTU slave id={args.slave_id} on {args.port} @ {args.baud} 8E1")
    StartSerialServer(
        context=context,
        port=args.port,
        baudrate=args.baud,
        bytesize=8,
        parity="E",
        stopbits=1,
        timeout=1,
    )


if __name__ == "__main__":
    main()
