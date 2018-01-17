0. System configuration
OS: Ubuntu 17.04
graphic: GTX1080
HMD: Oculus Rift CV1
Ubuntu runs in dual screen mode(HMD on HDMI-0, PC screen on DVI-5). See xorg.conf for example configuration.
These are mendatory options:
"
    Option         "AllowHMD" "yes"
    Option         "AllowEmptyInitialConfiguration" "true"
    Option         "ConnectedMonitor" "DP-5,HDMI-0"
"
Screen 0 shows ubuntu desktop, screen 1 is used by HMD.

1. Build
The following packages are needed plus OpenHMD

sudo apt-get install cmake
sudo apt-get install libtool
sudo apt-get install libudev-dev libusb-1.0-0-dev libfox-1.6-dev
sudo apt-get install libhidapi-dev

https://github.com/OpenHMD/OpenHMD
git clone https://github.com/OpenHMD/OpenHMD.git
cmake .
make -j32
sudo make install

sudo apt-get install ocl-icd-opencl-dev

sudo apt-get install libsdl2-dev
sudo apt-get install libglew-dev
sudo apt-get install freeglut3-dev

sudo apt-get install libboost-dev

sudo apt install x11-xserver-utils

mkdir build
cd build
cmake ../ && make -j32


2. Run
FYI: The app will call xrandr as follow in order to configure HMD
"xrandr --screen 1 --output HDMI-0 --auto". Make sure HMD is on HDMI-0.


DISPLAY=:0.1 ./server

