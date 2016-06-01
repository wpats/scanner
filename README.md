# scanner
This is a simple scanner application that currently supports the following SDR devices: Ettus B210, Nuand BladeRF, Airspy, 
SdrPlay, and HackRF. It is lightweight and fast compared to other scanner programs. It implements a thin and efficient 
abstraction layer to interface to each of the devices. Because of this it is very fast. I wrote this because existing 
programs from GNUradio like usrp_spectrum_sense and osmocom_spectrum_sense use the rather heavyweight GNUradio abstractions. 
Also they do not take avantage of devices specific features to improve efficiency.
