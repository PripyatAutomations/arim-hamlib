#!/bin/bash
#cd ardop2
#make
#sudo make instal
#cd ..
sudo apt -y install libncurses5-dev libhamlib-dev libasound2-dev
./configure --prefix=/usr
make
sudo make install
