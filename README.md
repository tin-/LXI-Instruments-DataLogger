# LXI Instruments Data Logger

## Introduction

LXI Instruments Data Logger is application for controlling LXI-compatible instruments, such as digital multimeters,  source measurement unit, electronic load, power sources, etc.

This application is based on open source [Liblxi](https://github.com/lxi-tools/liblxi) library.

I create these applications for use on my [Raspberry Pi Zero W](https://www.raspberrypi.org/products/raspberry-pi-zero-w/) board. It has many advantages, such as: autonomous data logging, very low power consumption, simple design and flexible configuration.


## Requriments and compile
- Liblxi 1.13
- Libconfig
- Pthread
- Libi2c
- Libncursesw
- GCC
- GNU Screen


Install requirements:
- apt-get install git screen libavahi-client3 libavahi-client-dev libxml2 libxml2-dev libconfig9 libconfig-dev libncursesw5 libncurses5-dev libncursesw5-dev libi2c-dev i2c-tools
- git clone https://github.com/shodanx/LXI-Instruments-DataLogger.git
- wget https://github.com/lxi-tools/liblxi/releases/download/v1.13/liblxi-1.13.tar.xz
- tar xf liblxi-1.13.tar.xz
- cd liblxi-1.13
- ./configure; make; make install


- Set correct UTF-8 locale at default in raspi-config.
- Run make.sh to compile the application.

## Usage
You can use run.sh script for start applitation into [GNU Screen](https://en.wikipedia.org/wiki/GNU_Screen) utility. It automaticaly run screen and attach last screen session. For screen deattach (autonomous use) you can press Ctrl-A D combination. At next run, script attach to last session automatically.
If screen resized use 'r' key to window update.
Press 'space' for pause measurements, or press 'q' for close application.

Measurements data saved into csv_save_dir. CSV file name generated automatically as current date and time.

Into 'channels' config section you can configure up to 16 different instruments for paralells measurements. Each instruments can be configured with different init-string, timeout, read-command, and connection settings.

For high-speed measurements(NPLC lower 0.1) you can configure refresh speed devider 'screen_refresh_div' for low CPU usage in screen refresh code. As example screen_refresh_div=100 has refreshed screen after each 100 measurements, it may take up to 2 times faster data receiving.

## Texas Instruments TMP117 temperature sensor

This application also support read temperature data from high precision TI TMP117 temperature sensor(up to 4 sensor on one I2C bus).
This feature tested only on Raspberry Pi Zero W and 3B+.

For enable "repeated start" sequence of TMP117 on Raspberry Pi you need do follow steps: 

 1. Enable i2c support in raspi-config.
 2. Add line "i2c-bcm2708" into /etc/modules
 3. Create file /etc/modprobe.d/i2c.conf with content "options i2c-bcm2708 combined=1"
 4. Reboot Raspberry Pi.

To minimize Self-Heating Effect with FlexPCB use 0x2A0 configuration word of TMP117.

## Screenshot

![](https://misrv.com/wp-content/uploads/2019/04/cons_dl.png)


## License and author

This code is released under GPL v3 license.

Author: Andrey Bykanov (aka Shodan)
E-Mail: adm@misrv.com
Location: Tula city, Russia.
