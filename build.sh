#!/bin/bash

cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_BUILD_TYPE=Debug -GNinja -Bbuild
cmake --build build

glslc -fshader-stage=vertex shader/vertex.glsl -o shader/vertex.spv
glslc -fshader-stage=fragment shader/fragment.glsl -o shader/fragment.spv
