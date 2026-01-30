#! bin/bash

git clone https://github.com/yarch3/OSLabsSem7.git

cd lab1/linux

mkdir -p build

cd build

cmake .
make

./lab1