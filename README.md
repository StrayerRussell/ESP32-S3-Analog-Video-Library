# ESP32-S3-Analog-Video-Library
Analog Video Library for ESP32-S3 (NTSC, PAL, VGA)

This is a library for the ESP32-S3 which uses the LCD peripheral to generate all manner of analog video. This is heavily based upon Bitluni's ESP32-S3-VGA library 
(https://github.com/bitluni/ESP32-S3-VGA), But Modified to support Composite Video. I intend to reimplement VGA into the library at some point (as well as document and
pretty up the code)

Working Features
Monochrome NTSC Video 720*480 and 960*540
Image drawing functions that make drawing images to external PSRAM much faster and more efficient
 - (onebitimage, monoimage)

Features in Development
Color NTSC (Need to make color encoding more efficient and get a better resistor DAC for testing fidelity)

Future Features (Hopefully)
Block Truncation Coding Image drawing function (Low memory footprint for almost 8-bit grayscale quality)
Reimplement VGA modes to make a unified library
PAL video modes
