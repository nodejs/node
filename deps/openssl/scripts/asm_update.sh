#!/bin/bash
# This script updates the OpenSSL asm files.
SCRIPTSDIR="`dirname \"$0\"`"

gcc_version=`gcc --version | grep ^gcc | awk '{print $3}'` 
nasm_version=`nasm -v | grep ^NASM | awk '{print $3}'` 

function generate {
    pushd $1
    make clean
    find . -iname '*.asm' -exec rm "{}" \;
    find . -iname '*.s' -exec rm "{}" \;
    find . -iname '*.S' -exec rm "{}" \;
    make
    popd
}

export CC=gcc
export ASM=nasm
generate ${SCRIPTSDIR}/../asm

unset CC
unset ASM
generate ${SCRIPTSDIR}/../asm_obsolete

pushd ${SCRIPTSDIR}/../
if git diff-index --quiet HEAD asm asm_obsolete ; then
    git add asm asm_obsolete
    git commit asm asm_obsolete -F- <<EOF
deps: update openssl asm and asm_obsolete files

Regenerate asm files with Makefile and CC=gcc and ASM=nasm where gcc
version was ${gcc_version} and nasm version was ${nasm_version}.

Also asm files in asm_obsolete dir to support old compiler and
assembler are regenerated without CC and ASM envs.
EOF
else 
  echo "No changes in asm or asm_obsolete. Nothing to commit"
fi

popd
