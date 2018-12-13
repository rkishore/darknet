#!/bin/bash

if [ "$#" -ne 1 ]; then
  echo "Usage  : $0 VERSION" >&2
  echo "Example: $0 1.3.1" >&2
  exit 1
fi

VERSION=$1

mkdir ~/debs/classifyapp-$VERSION/
cp -r src/ ~/debs/classifyapp-$VERSION/
cp -r include/ ~/debs/classifyapp-$VERSION/
#cp -r bin/ ~/debs/classifyapp-$VERSION/
cp Makefile ~/debs/classifyapp-$VERSION/
cp config.mak ~/debs/classifyapp-$VERSION/
cd ~/debs/classifyapp-$VERSION
make clean;
rm -rf debian;

cd ..
rm classifyapp-$VERSION.tar.gz classifyapp_$VERSION.orig.tar.gz
tar -pczf classifyapp-$VERSION.tar.gz classifyapp-$VERSION
cd classifyapp-$VERSION
dh_make -e kishore.ramachandran@igolgi.com -f ../classifyapp-$VERSION.tar.gz
cp control debian/control
dpkg-buildpackage -uc -us -d -b
