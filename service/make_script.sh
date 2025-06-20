#!/bin/sh
# set -eux

INSTALL_ABS_PATH=$(pwd)/_build
BUILD_DIR=cmake-build

mkdir -p ${INSTALL_ABS_PATH}
mkdir -p ${BUILD_DIR}
cd ${BUILD_DIR}

# CMAKE_BUILD_TYPE: Debug, Release, RelWithDebInfo and MinSizeRel
cmake -DCMAKE_VERBOSE_MAKEFILE=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel 4
cmake --install . --prefix ${INSTALL_ABS_PATH}

cd ..
rm -rf ${BUILD_DIR}
