#!/bin/bash

# This script copies source file lists from src/Makefile.am to cmake files.

get_variable_value() {
  local FILENAME=$1
  local VARNAME=$2
  awk "
    BEGIN { start = 0; }
    /^$VARNAME =/ { start = 1; }
    { if (start) { print \$0; } }
    /\\\\\$/ { next; }
    { start = 0; }
  " $FILENAME \
    | sed "s/^$VARNAME =//" \
    | sed "s/[ \\]//g" \
    | grep -v "^\\$" \
    | grep -v "^$" \
    | LC_ALL=C sort | uniq
}

get_header_files() {
  get_variable_value $@ | grep '\.h$'
}

get_source_files() {
  get_variable_value $@ | grep "cc$\|inc$"
}

get_proto_files_blacklisted() {
  get_proto_files $@ | sed '/^google\/protobuf\/unittest_enormous_descriptor.proto$/d'
}

get_proto_files() {
  get_variable_value $@ | grep "pb.cc$" | sed "s/pb.cc/proto/"
}

sort_files() {
  for FILE in $@; do
    echo $FILE
  done | LC_ALL=C sort | uniq
}

MAKEFILE=src/Makefile.am

[ -f "$MAKEFILE" ] || {
  echo "Cannot find: $MAKEFILE"
  exit 1
}

# Extract file lists from src/Makefile.am
GZHEADERS=$(get_variable_value $MAKEFILE GZHEADERS)
HEADERS=$(get_variable_value $MAKEFILE nobase_include_HEADERS)
PUBLIC_HEADERS=$(sort_files $GZHEADERS $HEADERS)
LIBPROTOBUF_LITE_SOURCES=$(get_source_files $MAKEFILE libprotobuf_lite_la_SOURCES)
LIBPROTOBUF_SOURCES=$(get_source_files $MAKEFILE libprotobuf_la_SOURCES)
LIBPROTOC_SOURCES=$(get_source_files $MAKEFILE libprotoc_la_SOURCES)
LIBPROTOC_HEADERS=$(get_header_files $MAKEFILE libprotoc_la_SOURCES)
LITE_PROTOS=$(get_proto_files $MAKEFILE protoc_lite_outputs)
PROTOS=$(get_proto_files $MAKEFILE protoc_outputs)
PROTOS_BLACKLISTED=$(get_proto_files_blacklisted $MAKEFILE protoc_outputs)
WKT_PROTOS=$(get_variable_value $MAKEFILE nobase_dist_proto_DATA)
COMMON_TEST_SOURCES=$(get_source_files $MAKEFILE COMMON_TEST_SOURCES)
COMMON_LITE_TEST_SOURCES=$(get_source_files $MAKEFILE COMMON_LITE_TEST_SOURCES)
TEST_SOURCES=$(get_source_files $MAKEFILE protobuf_test_SOURCES)
LITE_TEST_SOURCES=$(get_source_files $MAKEFILE protobuf_lite_test_SOURCES)
LITE_ARENA_TEST_SOURCES=$(get_source_files $MAKEFILE protobuf_lite_arena_test_SOURCES)
TEST_PLUGIN_SOURCES=$(get_source_files $MAKEFILE test_plugin_SOURCES)

################################################################################
# Update cmake files.
################################################################################

CMAKE_DIR=cmake
EXTRACT_INCLUDES_BAT=cmake/extract_includes.bat.in
[ -d "$CMAKE_DIR" ] || {
  echo "Cannot find: $CMAKE_DIR"
  exit 1
}

[ -f "$EXTRACT_INCLUDES_BAT" ] || {
  echo "Cannot find: $EXTRACT_INCLUDES_BAT"
  exit 1
}

set_cmake_value() {
  local FILENAME=$1
  local VARNAME=$2
  local PREFIX=$3
  shift
  shift
  shift
  awk -v values="$*" -v prefix="$PREFIX" "
    BEGIN { start = 0; }
    /^set\\($VARNAME/ {
      start = 1;
      print \$0;
      len = split(values, vlist, \" \");
      for (i = 1; i <= len; ++i) {
        printf(\"  %s%s\\n\", prefix, vlist[i]);
      }
      next;
    }
    start && /^\\)/ {
      start = 0;
    }
    !start {
      print \$0;
    }
  " $FILENAME > /tmp/$$
  cp /tmp/$$ $FILENAME
}


# Replace file lists in cmake files.
CMAKE_PREFIX="\${protobuf_source_dir}/src/"
set_cmake_value $CMAKE_DIR/libprotobuf-lite.cmake libprotobuf_lite_files $CMAKE_PREFIX $LIBPROTOBUF_LITE_SOURCES
set_cmake_value $CMAKE_DIR/libprotobuf.cmake libprotobuf_files $CMAKE_PREFIX $LIBPROTOBUF_SOURCES
set_cmake_value $CMAKE_DIR/libprotoc.cmake libprotoc_files $CMAKE_PREFIX $LIBPROTOC_SOURCES
set_cmake_value $CMAKE_DIR/libprotoc.cmake libprotoc_headers $CMAKE_PREFIX $LIBPROTOC_HEADERS
set_cmake_value $CMAKE_DIR/tests.cmake lite_test_protos "" $LITE_PROTOS
set_cmake_value $CMAKE_DIR/tests.cmake tests_protos "" $PROTOS_BLACKLISTED
set_cmake_value $CMAKE_DIR/tests.cmake common_test_files $CMAKE_PREFIX $COMMON_TEST_SOURCES
set_cmake_value $CMAKE_DIR/tests.cmake common_lite_test_files $CMAKE_PREFIX $COMMON_LITE_TEST_SOURCES
set_cmake_value $CMAKE_DIR/tests.cmake tests_files $CMAKE_PREFIX $TEST_SOURCES
set_cmake_value $CMAKE_DIR/tests.cmake lite_test_files $CMAKE_PREFIX $LITE_TEST_SOURCES
set_cmake_value $CMAKE_DIR/tests.cmake lite_arena_test_files $CMAKE_PREFIX $LITE_ARENA_TEST_SOURCES

# Generate extract_includes.bat
echo "mkdir include" > $EXTRACT_INCLUDES_BAT
for INCLUDE in $PUBLIC_HEADERS $WKT_PROTOS; do
  INCLUDE_DIR=$(dirname "$INCLUDE")
  while [ ! "$INCLUDE_DIR" = "." ]; do
    echo "mkdir include\\${INCLUDE_DIR//\//\\}"
    INCLUDE_DIR=$(dirname "$INCLUDE_DIR")
  done
done | sort | uniq >> $EXTRACT_INCLUDES_BAT
for INCLUDE in $PUBLIC_HEADERS $WKT_PROTOS; do
  WINPATH=${INCLUDE//\//\\}
  echo "copy \"\${PROTOBUF_SOURCE_WIN32_PATH}\\..\\src\\$WINPATH\" include\\$WINPATH" >> $EXTRACT_INCLUDES_BAT
done

################################################################################
# Update bazel BUILD files.
################################################################################

set_bazel_value() {
  local FILENAME=$1
  local VARNAME=$2
  local PREFIX=$3
  shift
  shift
  shift
  awk -v values="$*" -v prefix="$PREFIX" "
    BEGIN { start = 0; }
    /# AUTOGEN\\($VARNAME\\)/ {
      start = 1;
      print \$0;
      # replace \$0 with indent.
      sub(/#.*/, \"\", \$0)
      len = split(values, vlist, \" \");
      for (i = 1; i <= len; ++i) {
        printf(\"%s\\\"%s%s\\\",\n\", \$0, prefix, vlist[i]);
      }
      next;
    }
    start && /\]/ {
      start = 0
    }
    !start {
      print \$0;
    }
  " $FILENAME > /tmp/$$
  cp /tmp/$$ $FILENAME
}


BAZEL_BUILD=./BUILD
BAZEL_PREFIX="src/"
if [ -f "$BAZEL_BUILD" ]; then
  set_bazel_value $BAZEL_BUILD protobuf_lite_srcs $BAZEL_PREFIX $LIBPROTOBUF_LITE_SOURCES
  set_bazel_value $BAZEL_BUILD protobuf_srcs $BAZEL_PREFIX $LIBPROTOBUF_SOURCES
  set_bazel_value $BAZEL_BUILD protoc_lib_srcs $BAZEL_PREFIX $LIBPROTOC_SOURCES
  set_bazel_value $BAZEL_BUILD lite_test_protos "" $LITE_PROTOS
  set_bazel_value $BAZEL_BUILD well_known_protos "" $WKT_PROTOS
  set_bazel_value $BAZEL_BUILD test_protos "" $PROTOS
  set_bazel_value $BAZEL_BUILD common_test_srcs $BAZEL_PREFIX $COMMON_TEST_SOURCES
  set_bazel_value $BAZEL_BUILD test_srcs $BAZEL_PREFIX $TEST_SOURCES
  set_bazel_value $BAZEL_BUILD test_plugin_srcs $BAZEL_PREFIX $TEST_PLUGIN_SOURCES
else
  echo "Skipped BUILD file update."
fi

