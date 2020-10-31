#!/usr/bin/env bash

# DO NOT use this script manually! Called by docker.

set -ex

# Print usage and fail.
function usage() {
  echo "Usage: protobuf_optimized_pip.sh PROTOBUF_VERSION" >&2
  exit 1   # Causes caller to exit because we use -e.
}

# Build wheel
function build_wheel() {
  PYTHON_VERSION=$1
  PYTHON_BIN=/opt/python/${PYTHON_VERSION}/bin/python

  $PYTHON_BIN setup.py bdist_wheel --cpp_implementation --compile_static_extension
  auditwheel repair dist/protobuf-${PROTOBUF_VERSION}-${PYTHON_VERSION}-linux_x86_64.whl
}

# Validate arguments.
if [ $0 != ./protobuf_optimized_pip.sh ]; then
  echo "Please run this script from the directory in which it is located." >&2
  exit 1
fi

if [ $# -lt 1 ]; then
  usage
  exit 1
fi

PROTOBUF_VERSION=$1
PYPI_USERNAME=$2
PYPI_PASSWORD=$3

DIR=${PWD}/'protobuf-python-build'
PYTHON_VERSIONS=('cp27-cp27mu' 'cp33-cp33m' 'cp34-cp34m' 'cp35-cp35m' 'cp36-cp36m')

mkdir -p ${DIR}
cd ${DIR}
curl -SsL -O https://github.com/protocolbuffers/protobuf/archive/v${PROTOBUF_VERSION}.tar.gz
tar xzf v${PROTOBUF_VERSION}.tar.gz
cd $DIR/protobuf-${PROTOBUF_VERSION}

# Autoconf on centos 5.11 cannot recognize AC_PROG_OBJC.
sed -i '/AC_PROG_OBJC/d' configure.ac
sed -i 's/conformance\/Makefile//g' configure.ac

# Use the /usr/bin/autoconf and related tools to pick the correct aclocal macros
export PATH="/usr/bin:$PATH"

# Build protoc
./autogen.sh
CXXFLAGS="-fPIC -g -O2" ./configure
make -j8
export PROTOC=$DIR/src/protoc

cd python

for PYTHON_VERSION in "${PYTHON_VERSIONS[@]}"
do
  build_wheel $PYTHON_VERSION
done

/opt/python/cp27-cp27mu/bin/twine upload wheelhouse/*
