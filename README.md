deepin-wm-switcher
===
deepin window manager monitoring and auto-switching service.

It is capable of:
+ monitoring health of 3d wm and falling back to 2d if bad things happened.
+ detecting platform capability and choose 2d/3d wm accordingly.

dependencies
===
those packages are required:
+ libx11-6
+ libx11-xcb1
+ libqt5gui5
+ libqt5x11extras5
+ libxcb-keysyms1
+ libglib2.0-0

those are needed to be built:
+ libx11-dev
+ libx11-xcb-dev
+ libqt5x11extras5-dev
+ qtbase5-dev
+ libxcb-keysyms1-dev
+ libglib2.0-dev

build and install
===
``
make build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
make
make install
``

