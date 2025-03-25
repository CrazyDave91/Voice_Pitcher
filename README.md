# Voice Pitcher

This Project implements an Arduino Uno based voice pitcher with mic input and speaker output.

The core parts are listed in the BOM folder. All other parts are things i had lying around:
- BC337-40 NPN Transistor for the speaker driver
- 1N4001 Freewheeling Diode for the speaker driver
- Some random capacitors for filtering the supply and potentiometer output
- Some random jumper cables
- The screws used are M3 maschine screws

# Updates
**20250325**: The pitcher now supports two modes. Default Mode (knob on high pitch at startup), where the knob controls the pitch and Binary Mode (knob on low pitch at startup), where only one octave lower or higher can be selected. The mode is selected at startup depending on the knobs position and seperated by its half point.

# Example Setup
![Voice Pitcher Assembly](Voice_Pitcher.jpg)
