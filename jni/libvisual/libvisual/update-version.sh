#!/bin/sh

GIT="/usr/bin/git"

SOURCE_DIR="`pwd`/jni/libvisual/libvisual"
BINARY_DIR="`pwd`/jni/libvisual/libvisual"

INPUT_HEADER="${SOURCE_DIR}/version.h.in"
OUTPUT_HEADER="${BINARY_DIR}/version.h"

cd "${SOURCE_DIR}"

REVISION=$(${GIT} describe --always --dirty 2>/dev/null)
[ -z "${REVISION}" ] && REVISION="unknown"

if [ -e ${OUTPUT_HEADER} ] ; then
    OLD_REVISION=$(grep LV_REVISION < ${OUTPUT_HEADER} 2>/dev/null)
    OLD_REVISION=${OLD_REVISION#*\"}
    OLD_REVISION=${OLD_REVISION%\"*}
fi

if [ "${OLD_REVISION}" != "${REVISION}" ]; then
    echo "Generating version.h"
    sed -e "s/\@LV_REVISION\@/${REVISION}/g" < ${INPUT_HEADER} > ${OUTPUT_HEADER}
fi
