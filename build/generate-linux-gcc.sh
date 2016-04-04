#!/bin/sh

DIR=linux-gcc
GEN="Unix Makefiles"

mkdir -p ${DIR}/debug
mkdir -p ${DIR}/release

cd ${DIR}/debug
cmake -G "${GEN}" -DCMAKE_BUILD_TYPE=Debug ../../..
cd -

cd ${DIR}/release
cmake -G "${GEN}" -DCMAKE_BUILD_TYPE=Release ../../..
cd -
