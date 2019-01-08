Proof of concept for transcoding on RPI.
The modify base on omxplayer by adding encoder component & processing output stream/file.

usage:

./omxplayer input output

input: url streaming, file
output:
 - file
 - udp://address:port


--- Original README of omxplayer from here ---

omxplayer(1) -- Raspberry Pi command line OMX player
====================================================

OMXPlayer is a commandline OMX player for the Raspberry Pi. It was developed as
a testbed for the XBMC Raspberry PI implementation and is quite handy to use
standalone. 

## DOWNLOADING

    git clone https://github.com/popcornmix/omxplayer.git

## HELP AND DOCS

omxplayer's built-in help and the man page are all generated from this
README.md file during make. You may need to change the Makefile
if you modify the structure of README.md!

## COMPILING

Run this script which will install build dependency packages,
including g++ 4.7, and update firmware

    ./prepare-native-raspbian.sh

Build with

    make ffmpeg
    make

Install with
    
    sudo make install

## CROSS COMPILING

You need the content of your sdcard somewhere mounted or copied. There might be
development headers to install on the running Pi system for the crosscompiling.

Edit Makefile.include and change the settings according your locations.

    make ffmpeg
    make
    make dist
