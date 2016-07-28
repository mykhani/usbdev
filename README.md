# Firmware and Linux driver for TI's TM4C123G Eval kit based custom USB device

The aim of this project was to develop hands-on understanding of USB protocol by developing
a custom USB device on ARM cortex-M4 based Texas Instruments TM4C123G microcontroller.

USB controller was programmed from scratch with some help taken from TI's Tivaware USB library.

TM4C123G USB controller provides USB OTG functionality but it is programmed to be in device mode.

Wireshark was used to observe USB packets during USB enumeration, configuration and data transfer.

Two endpoints EP1_IN and EP1_OUT are configured on device side to communicate with USB host
using interrupt transfers.

A Linux driver is written to communicate with this device. Right now the driver doesn't do much
and just sends some data over EP1_OUT and receives data over EP1_IN. 
