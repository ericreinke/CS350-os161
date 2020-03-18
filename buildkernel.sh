#!/bin/bash
cd ~/cs350-os161/os161-1.99/kern/conf
./config ASST2
cd ../
bmake depend
cd compile/ASST2
bmake depend
bmake
bmake install
cd ~/cs350-os161/os161-1.99
bmake
bmake install
cd ../root/
echo "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
sys161 kernel-ASST2
