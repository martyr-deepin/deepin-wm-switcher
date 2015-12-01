## Install debug pacakges
``
sudo apt-get install gdb libx11-dbg libx11-xcb-dbg \
         libqt5x11extras5-dbg qtbase5-dbg libxcb-keysyms1-dbg \
         libglib2.0-0-dbg
         ``
## Debug in source directory
         ``
         cmake -DCMAKE_BUILD_TYPE=Debug .
         make
         gdb ./src/deepin-wm-switcher
         ``

