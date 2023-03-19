# MoonTracker_GUI
Python GUI for the Moontracker controller

This should run on pretty much anything that can run Python in a graphical enviroment.

I choose to run it on a PC to keep shack clutter low, but you could equally well run it on a RpaspberryPi with a touchscreen for a stand alone controller.

It uses the "skyfield" API for ephemeris data and communicates with the hardware over serial, I use RS485 for robustness, and operate the hardware remotely, but again, if you prefer things to be "in the shack", you could run the hardware locally over a USB cable and send the motordrives and feedback over longer wires to the dish, I prefer to keep those short and use serial for the long haul, but again, its entirely up to you.

The display is simple enough,  shows you the target you have selected, rise and set time, and a readout tracks the position of the object and where your dish is currently pointing.

"align" causes the current position to become the target position, so just select "moon" or some other object, jog the dish until you are happy with the current alignment, press "Align" and the current position and target position will become equal.
 Pressing "Track" will cause the dish to then actively track the object using the offsets just set.  The offsets are stored in EEPROM on the controller, so they will be stored next time you connect.
 
