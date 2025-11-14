#!/bin/bash

cd `dirname $0` || exit 1
    echo Deleting.....
rm -rf exprtk-0.0.3
if [ ! -r exprtk.zip ]; then
    echo Downloading.....
    curl -L https://github.com/ArashPartow/exprtk/archive/refs/tags/0.0.3.zip -o exprtk.zip
fi
echo Unzipping.....
unzip -x exprtk
rm -rf exprtk
mv exprtk-0.0.3 exprtk
echo Patching.....
for PATCH in patches/*.patch; do
    patch -b -p0 < ${PATCH}
done
