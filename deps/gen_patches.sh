#!/bin/bash

cd `dirname $0` || exit 1
for ORIG in `find exprtk -name *.orig`; do
    MODIFIED=${ORIG%.orig}
    echo ${MODIFIED}
    diff -bu  ${ORIG} ${MODIFIED} > patches/`basename ${MODIFIED}`.patch
done