


# Release Notes for <mark>MX_WIFI Component Driver</mark>
Copyright &copy; 2020 STMicroelectronics\
Microcontrollers Division - Application Team

[![ST logo](_htmresc/st_logo_2020.png)](https://www.st.com)


# Purpose

This directory contains the Wi-Fi component driver running on the STM32 host for the MXCHIP's EMW3080B Wi-Fi module.


# Update History

<label for="collapse-section13" aria-hidden="false">V2.3.5 / 20-April-2026</label>
<div>

## Main changes

  - Replace `MX_WIFI_BARE_OS_H` usage as a compilation switch with `MX_WIFI_BARE_OS`.
  - Fix errors seen with static analyser and C++ compilers.

## EMW3080B MXCHIP Wi-Fi module firmware

  - The EMW3080B MXCHIP Wi-Fi module firmware version has to be <mark>V2.3.4</mark> and
  the way to update your board with it is available at [X-WIFI-EMW3080B](https://www.st.com/en/development-tools/x-wifi-emw3080b.html).

</div>


<label for="collapse-section12" aria-hidden="true">V2.3.4 / 12-December-2022</label>
<div>

## Main changes

  - Fix TCP server in softAP and STA mode.
  - Add Core/mx_address.[ch] files.
  - Add new WiFi connect API MX_WIFI_Connect_Adv.
  - Add new API for EAP, WPS, configuration server.
  - Support get MAC for softAP.
  - Fix some MISRA errors.
  - Update WiFi API documents for parameters and return value.

## EMW3080B MXCHIP Wi-Fi module firmware

  - The EMW3080B MXCHIP Wi-Fi module firmware version has to be <mark>V2.3.4</mark> and 
  the way to update your board with it is available at [X-WIFI-EMW3080B](https://www.st.com/en/development-tools/x-wifi-emw3080b.html).

</div>


<label for="collapse-section11" aria-hidden="true">V2.1.13 / 20-June-2022</label>
<div>

## Main changes

  - FIX spelling Errors

##

</div>


<label for="collapse-section10" aria-hidden="true">V2.1.12 / 16-February-2022</label>
<div>

## Main changes

  - FIX a C++ Compilation Error

## Limitation

  - Access point mode with the TCP/IP mode on STM32 host is not functional. It works when TCP/IP runs on the EMW3080B module.

## EMW3080B MXCHIP Wi-Fi module firmware

  - The EMW3080B MXCHIP Wi-Fi module firmware version has to be V2.1.11 and the way to update your board with it is available at [X-WIFI-EMW3080B](https://www.st.com/en/development-tools/x-wifi-emw3080b.html).
  - For the B-U585I-IOT02A rev C. board, this EMW3080B MXCHIP Wi-Fi module firmware version V2.1.11 is installed by default and you do not need to update it.

</div>


<label for="collapse-section9" aria-hidden="true">V2.1.11 / 17-June-2021</label>
<div>

## Main changes

  - Update the way to declare licenses
  - Fix MISRA and Codesonar errors
  - Create IO pattern files for UART and SPI interface
  - Address some code alignment and cosmetics

## Limitation
  - Access point mode with the TCP/IP mode on STM32 host is not functional. It works when TCP/IP runs on the EMW3080B module.

</div>


<label for="collapse-section8" aria-hidden="true">V2.1.10 / 8-April-2021</label>
<div>

## Main changes

  - Clean configuration for FreeRTOS and Bare metal support
  - Add Support for MXCHIP Firmware 2.1.10 (adding updater)
  - Driver is backward compatible to 2.1.9 firmware
  - Tested on U5 Discovery board Rev C

##

</div>


<label for="collapse-section7" aria-hidden="true">V2.1.9 / 30-March-2021</label>
<div>

## Main changes

  - Strongly improve performances and stability
  - Add SPI DMA support
  - Fix Access Point mode support
  - Tested on U5 Discovery board Rev C

##

</div>


<label for="collapse-section6" aria-hidden="true">V2.1.6 / 09-February-2021</label>
<div>

## Main changes

  - Add Error mngt
  - Remove Circular buffer, use directly Pbuf
  - Improve debug msg and echo server
  - Fix memory overrun
  - Adding debug support
  - SPI protocol fix
  - SPI checksum fix
  - Improve OS porting, change LOCK
  - Improve Malloc and Messages Macros

##

</div>


<label for="collapse-section5" aria-hidden="true">V2.1.4 / 08-February-2021</label>
<div>

## Main changes

  - Overall driver rework
  - SPI fix
  - Race condition fixes
  - Optimized memory usage

##

</div>


<label for="collapse-section4" aria-hidden="true">V1.4.0 / 18-November-2020</label>
<div>

## Main changes

  - Add new API getsockname, getpeername

##

</div>


<label for="collapse-section3" aria-hidden="true">V1.3.0 / 05-November-2020</label>
<div>

## Main changes

  - Hardware flow control: removed the flow pin usage
  - MISRA-C_2012 compliant
  - AStyle run
  - Fixed CodeSonar warnings
  - Licensed under ST license SLA0044 (the "License")

##

</div>


<label for="collapse-section2" aria-hidden="true">V1.0.4 / 15-May-2020 </label>
<div>

## Main changes

  - First official release of MX_WIFI WiFi component.

##

</div>



For complete documentation on <mark>STM32</mark> microcontrollers please visit <http://www.st.com/stm32>