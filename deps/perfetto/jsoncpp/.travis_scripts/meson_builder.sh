#!/usr/bin/env sh
# This script can be used on the command line directly to configure several
# different build environments.
# This is called by `.travis.yml` via Travis CI.
# Travis supplies $TRAVIS_OS_NAME.
#  http://docs.travis-ci.com/user/multi-os/
# Our .travis.yml also defines:

#   - BUILD_TYPE=release/debug
#   - LIB_TYPE=static/shared

env_set=1
if ${BUILD_TYPE+false}; then
  echo "BUILD_TYPE not set in environment."
  env_set=0
fi
if ${LIB_TYPE+false}; then
  echo "LIB_TYPE not set in environment."
  env_set=0
fi
if ${CXX+false}; then
  echo "CXX not set in environment."
  env_set=0
fi


if [ ${env_set} -eq 0 ]; then
  echo "USAGE:  CXX=$(which clang++)  BUILD_TYPE=[release|debug] LIB_TYPE=[static|shared] $0"
  echo ""
  echo "Examples:"
  echo "           CXX=$(which clang++) BUILD_TYPE=release LIB_TYPE=shared DESTDIR=/tmp/meson_json_cpp $0"
  echo "           CXX=$(which clang++) BUILD_TYPE=debug   LIB_TYPE=shared DESTDIR=/tmp/meson_json_cpp $0"
  echo "           CXX=$(which clang++) BUILD_TYPE=release LIB_TYPE=static DESTDIR=/tmp/meson_json_cpp $0"
  echo "           CXX=$(which clang++) BUILD_TYPE=debug   LIB_TYPE=static DESTDIR=/tmp/meson_json_cpp $0"

  echo "           CXX=$(which g++)     BUILD_TYPE=release LIB_TYPE=shared DESTDIR=/tmp/meson_json_cpp $0"
  echo "           CXX=$(which g++)     BUILD_TYPE=debug   LIB_TYPE=shared DESTDIR=/tmp/meson_json_cpp $0"
  echo "           CXX=$(which g++)     BUILD_TYPE=release LIB_TYPE=static DESTDIR=/tmp/meson_json_cpp $0"
  echo "           CXX=$(which g++)     BUILD_TYPE=debug   LIB_TYPE=static DESTDIR=/tmp/meson_json_cpp $0"

  exit -1
fi

if ${DESTDIR+false}; then
  DESTDIR="/usr/local"
fi

# -e: fail on error
# -v: show commands
# -x: show expanded commands
set -vex


env | sort

which python3
which meson
which ninja
echo ${CXX}
${CXX} --version
python3 --version
meson --version
ninja --version
_COMPILER_NAME=`basename ${CXX}`
_BUILD_DIR_NAME="build-${BUILD_TYPE}_${LIB_TYPE}_${_COMPILER_NAME}"

./.travis_scripts/run-clang-format.sh
meson --fatal-meson-warnings --werror --buildtype ${BUILD_TYPE} --default-library ${LIB_TYPE} . "${_BUILD_DIR_NAME}"
ninja -v -j 2 -C "${_BUILD_DIR_NAME}"

cd "${_BUILD_DIR_NAME}"
  meson test --no-rebuild --print-errorlogs

  if [ "${DESTDIR}" != "/usr/local" ]; then
     ninja install
  fi
cd -

if ${CLEANUP+false}; then
  echo "Skipping cleanup:  build directory will persist."
else
  rm -r "${_BUILD_DIR_NAME}"
fi
