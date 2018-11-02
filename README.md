---
# CC1310 Sensor Hub

---

## Summary

Custom sensor hub utilizing CC1310 Launchpad and TI-RTOS. Hub communicates with server
utilizing UART and custom command protocol. Project is used in Code Composer Studio.

## Command Protocol

All commands end with line feed character '\n'  
Reference can be sent before command in format <'ref'>'command', Hub will reply in same format  
Example  
cmnd: <56>A#0123456789ABCDEF#  
resp: <56>A#0123456789ABCDEF#0#  
Address is 8-byte IEEE address as hex number  

|Command          |Hub Receives     |Response                              |
|-----------------|-----------------|--------------------------------------|
|Add Node         |A#'address'#     |A#'address'#'ref num'#                |
|Remove Node      |R#'ref num'#     |R#'address'#                          |
|Data Request     |D#'ref num'#     |D#'raw data'#'epoch time'#'ref num'#  |
|Hub Temperature  |L##              |L#'temp &deg;C','temp &deg;F'#        |
|Get Hub Time     |t#G#             |t#G'epoch time'#                      |
|Set Hub Time     |t#'epoch time'#  |t#'epoch time'#                       |
|Check reference  |C#'ref num'#     |C#'address'#'ref num'#                |
|Get all refs     |f##              |f#'ref 1','ref 2',...,'ref n'#        | 
|Garage Command   |G#C#'ref num'#'open or close'#  |G#C#'address'#'status'#'epoch time'#  |
