#!/bin/bash

cd `dirname $0` || exit 1
    echo Deleting.....
rm -rf exprtk
if [ ! -r exprtk.zip ]; then
    echo Downloading.....
    curl https://www.partow.net/downloads/exprtk.zip -o exprtk.zip
fi
echo Unzipping.....
unzip -x exprtk
echo Patching.....
for PATCH in patches/*.patch; do
    patch -b -p0 < ${PATCH}
done
