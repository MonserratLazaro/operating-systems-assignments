#!/bin/sh

cd ~/

echo "Creando folder..."
mkdir NewFolder

echo ""
echo "Folders en ~/"
ls -d */

mv NewFolder ~/Desktop

echo ""
echo "Folders después de mover NewFolder"
ls -d */

cd Desktop
echo ""
echo "Folders en Desktop"
ls -d */

rmdir NewFolder 
echo ""
echo "Folders en Desktop después de elimar NewFolder"
ls -d */

echo ""
read -p "Presione una tecla para salir"
