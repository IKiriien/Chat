#!/usr/bin/env bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

rm -rf build

cmake -E make_directory build
cd build
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ../
cmake --build . --target Server --config Release -- -j 4
