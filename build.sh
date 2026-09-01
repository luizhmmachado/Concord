#!/bin/bash
set -e

cd /home/luizmachado/Concord/build/Qt_5_15_4_Desktop_Linux_gcc_64-Debug

echo "=== Cleaning old build ==="
rm -f main.o moc_main.cpp moc_main.o Concord

echo "=== Regenerating Makefile ==="
qmake ../../Concord.pro

echo "=== Compiling ==="
make -j4

echo "=== Build complete ==="
echo "Running Concord with debug output..."
./Concord
