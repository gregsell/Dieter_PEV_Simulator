# dieter PEV simulator

**Hardware and Firmware for the Dieter PEV Simulator Board**

This board is a physical interface between the [EVerest PEV Simulator](https://github.com/gregsell/DieterEvDriver/) software running on linux and a real CCS charging station. It generates and measures the IEC 61851 Control Pilot (CP) signal, drives the contactor and plug lock, and measures current and voltage on the HV lines.
The board is designed for DC charging test setups and capable of handling very high voltages.

The name and concept come from [uhi22/dieter](https://github.com/uhi22/dieter), a hardware design by Uwe Hennig. That project uses [pyPLC](https://github.com/uhi22/pyPLC) as simulation software. This repository adapts the hardware and uses an [EVerest](https://everest.github.io)-based software stack, which takes over the protocol logic from pyPLC.


## System Overview

![hadware overview](/docs/sys_arch_hw.svg)

## Hardware Components

The Arduino handles all low-level I/O relevant for CCS communication:

- **CP PWM measurement**: Fixed 5% duty cycle at 1 kHz (per IEC 61851-1) signals switch to high level communication by the EVSE
- **CP state control**: Sets CP voltage level to signal current state to EVSE during low-level communication
- **Contactor control**: Closes/opens the DC contactors on command from EVerest
- **Connector lock actuator**: Engages/releases the mechanical connector lock
- **Power meter**: Measures HV voltage and current

The Arduino communicates with the Linux host over UART via USB using a simple text-based protocol, as implemented in the [`DieterEvDriver`](https://github.com/gregsell/DieterEvDriver/tree/main/modules/DieterEvDriver) EVerest module.

### HomePlug PLC Modem

In this project a patched TPlink TL-PA4010P v5.0 used. (In the pyPLC project several modems were tested, see https://github.com/uhi22/pyPLC/blob/master/doc/hardware.md.) The necessary modification steps are available there as well.   

The modem connects to the CP line and handles the high-level communication necessary for CCS. The Linux host runs `EvSlac` (EVerest module) for SLAC matching and PyEvJosev for the V2G ISO 15118-2 session over PLC.


## Repository Structure
```
├── docs
│   ├── mounting                                    # 3d printable files of DIN rail mounts
│   │   ├── Dieter_box_DIN_rail_adapter.step
│   │   ├── L298N_board_DIN_rail_adapter.step
│   │   └── TL-PA4010P_v5.0_DIN_rail_adapter.step
│   ├── pcb                                         # KiCAD source files of PCB
│   │   ├── Dieter_PEV_Simulator.kicad_pcb
│   │   ├── Dieter_PEV_Simulator.kicad_pro
│   │   └── Dieter_PEV_Simulator.kicad_sch
│   ├── pdf                                         # pdf schematic
│   │   └── Dieter_PEV_Simulator_schematic.pdf
│   └── sys_arch_hw.svg
├── platformio.ini                                  # platformio configuration file
├── README.md                                       # this file
└── src
    └── main.cpp                                    # sourcecode
```

---

## Arduino Serial Protocol
The Arduino exposes a simple newline-delimited text protocol over serial. The `DieterEvDriver` EVerest module running on the linux host speaks this protocol.
```
commands (host -> MCU)
  keys                   possible values            
  set_contactor             {0, 1}                            
  set_connector_lock        {0, 1}   
  set_state_c               {0, 1}

measurements (MCU -> host)
  u_inlet                   {0, 1, 2...}
  current                   {0.0, 1.0, 2.0,..}
  cp_duty_cycle             {0, 1, ..99, 100}
  connector_lock_confirmed  {0, 1}

each newline-terminated and separated by colon.

examples:
  "set_contactor:0\n"
  "set_cp_state:C\n"
  "u_inlet:800\n"
```
**notes:**
- `current` is the only `float`, all the others are `integer`
- `set_state_c` might be misleading. In general there are more states, but this is the only one the EV can signal.
- EVerest itself does not need current and voltage measurements

## Mounting
Mounting everything to a DIN rail (german "Hutschiene") makes it possible to use stock parts and be flexible with arrangement.

For the single PCBs adaptors were 3d printed to work with this design: https://www.printables.com/model/161740-din-rail-clip  
The source files are available under [/docs/mounting]().

Each clip can hold up to three M3 nuts spaced 9 mm apart and can be attached (and removed!) from the rail with a screwdriver. 

---

## Related Projects and useful links

- [gregsell/EVerest PEV simulator]() — EVerest software that runs on the host and controls this board
- [uhi22/dieter](https://github.com/uhi22/dieter) — Original Dieter reference hardware and pyPLC-based software
- [uhi22/pyPLC](https://github.com/uhi22/pyPLC) — Python PLC/V2G stack, first open-source CCS implementation
- [uhi22/foccci](https://github.com/uhi22/foccci) — open-source CCS controller, developed by openinverter community
- [EVerest](https://github.com/EVerest/everest-core) — EV charging framework
- [charIn CCS design guide](https://www.charin.global/media/pages/technology/ccs-specification/c07034e41e-1626949173/design_guide_combined_charging_system_v7.pdf) — very useful! (Almost) everything you ever need to know about CCS
