#!/bin/bash

cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -GNinja -Bbuild
cmake --build build

glslc shader/shader.vert -o shader/vert.spv
glslc shader/shader.frag -o shader/frag.spv
