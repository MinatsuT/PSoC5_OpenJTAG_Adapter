# PSoC5 OpenJTAG Adapter

This is an OpenJTAG implementation for [PSoC 5LP Prototyping Kit (CY8CKIT-059)](http://japan.cypress.com/?rid=108038).

- P12_0 --> TDI
- P12_1 <-- TDO
- P12_2 --> TCK
- P12_3 --> TMS
- P12_4 --- (not used)
- P12_5 --> TRST (target reset signal, active low.)
- P1_7  <-- VTref (target voltage reference.)

Connect PSoC 5LP Prototyping Kit's MicroUSB to the debug host that runs OpenOCD.

You can monitor and control via KitProg's COM port.

TRST is an optional hard reset signal.

The output voltage of the TDI/TCK/TMS can be selected between the external reference mode and the internal 3.3V mode. After powering on, the external reference mode is selected. In the external reference mode, the voltage of the VTref connected to the target VCC is used as the output voltage. You can switch to the internal 3.3V mode by sending 'i' from the KitProg's COM port. By sending 'e', it will return to the external reference mode.
