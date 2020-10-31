#!/usr/bin/env sh
# This script can be used on the command line directly to configure several
# different build environments.
# This is called by `.travis.yml` via Travis CI.
# Travis supplies $TRAVIS_OS_NAME.
#  http://docs.travis-ci.com/user/multi-os/
# Our .travis.yml also defines:

#   - BUILD_TYPE=Release/Debug
#   - LIB_TYPE=static/shared
#
# Optional environmental variables
#   - DESTDIR <- used for setting the install prefix
#   - BUILD_TOOL=["Unix Makefile"|"Ninja"]
#   - BUILDNAME <- how to identify this build on the dashboard
#   - DO_MemCheck <- if set, try to use valgrind
#   - DO_Coverage <- if set, try to do dashboard coverage testing
#

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
  echo "USAGE:  CXX=$(which clang++)  BUILD_TYPE=[Release|Debug] LIB_TYPE=[static|shared] $0"
  echo ""
  echo "Examples:"
  echo "           CXX=$(which clang++) BUILD_TYPE=Release LIB_TYPE=shared DESTDIR=/tmp/cmake_json_cpp $0"
  echo "           CXX=$(which clang++) BUILD_TYPE=Debug   LIB_TYPE=shared DESTDIR=/tmp/cmake_json_cpp $0"
  echo "           CXX=$(which clang++) BUILD_TYPE=Release LIB_TYPE=static DESTDIR=/tmp/cmake_json_cpp $0"
  echo "           CXX=$(which clang++) BUILD_TYPE=Debug   LIB_TYPE=static DESTDIR=/tmp/cmake_json_cpp $0"

  echo "           CXX=$(which g++)     BUILD_TYPE=Release LIB_TYPE=shared DESTDIR=/tmp/cmake_json_cpp $0"
  echo "           CXX=$(which g++)     BUILD_TYPE=Debug   LIB_TYPE=shared DESTDIR=/tmp/cmake_json_cpp $0"
  echo "           CXX=$(which g++)     BUILD_TYPE=Release LIB_TYPE=static DESTDIR=/tmp/cmake_json_cpp $0"
  echo "           CXX=$(which g++)     BUILD_TYPE=Debug   LIB_TYPE=static DESTDIR=/tmp/cmake_json_cpp $0"

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

which cmake
cmake --version

echo ${CXX}
${CXX} --version
_COMPILER_NAME=`basename ${CXX}`
if [ "${BUILD_TYPE}" == "shared" ]; then
  _CMAKE_BUILD_SHARED_LIBS=ON
else
  _CMAKE_BUILD_SHARED_LIBS=OFF
fi

CTEST_TESTING_OPTION="-D ExperimentalTest"
#   - DO_MemCheck <- if set, try to use valgrind
if ! ${DO_MemCheck+false}; then
   valgrind --version
   CTEST_TESTING_OPTION="-D ExperimentalMemCheck"
else
#   - DO_Coverage <- if set, try to do dashboard coverage testing
  if ! ${DO_Coverage+false}; then
     export CXXFLAGS="-fprofile-arcs -ftest-coverage"
     export LDFLAGS="-fprofile-arcs -ftest-coverage"
     CTEST_TESTING_OPTION="-D ExperimentalTest -D ExperimentalCoverage"
     #gcov --version
  fi
fi

#  Ninja                        = Generates build.ninja files.
if ${BUILD_TOOL+false}; then
  BUILD_TOOL="Ninja"
  export _BUILD_EXE=ninja
  which ninja
  ninja --version
else
#  Unix Makefiles               = Generates standard UNIX makefiles.
  export _BUILD_EXE=make
fi

_BUILD_DIR_NAME="build-cmake_${BUILD_TYPE}_${LIB_TYPE}_${_COMPILER_NAME}_${_BUILD_EXE}"
mkdir -p ${_BUILD_DIR_NAME}
cd "${_BUILD_DIR_NAME}"
  if ${BUILDNAME+false}; then
     _HOSTNAME=`hostname -s`
     BUILDNAME="${_HOSTNAME}_${BUILD_TYPE}_${LIB_TYPE}_${_COMPILER_NAME}_${_BUILD_EXE}"
  fi
  cmake \
    -G "${BUILD_TOOL}" \
    -DBUILDNAME:STRING="${BUILDNAME}" \
    -DCMAKE_CXX_COMPILER:PATH=${CXX} \
    -DCMAKE_BUILD_TYPE:STRING=${BUILD_TYPE} \
    -DBUILD_SHARED_LIBS:BOOL=${_CMAKE_BUILD_SHARED_LIBS} \
    -DCMAKE_INSTALL_PREFIX:PATH=${DESTDIR} \
    ../

  ctest -C ${BUILD_TYPE} -D ExperimentalStart -D ExperimentalConfigure -D ExperimentalBuild ${CTEST_TESTING_OPTION} -D ExperimentalSubmit
  # Final step is to verify that installation succeeds
  cmake --build . --config ${BUILD_TYPE} --target install

  if [ "${DESTDIR}" != "/usr/local" ]; then
     ${_BUILD_EXE} install
  fi
cd -

if ${CLEANUP+false}; then
  echo "Skipping cleanup: build directory will persist."
else
  rm -r "${_BUILD_DIR_NAME}"
fi
