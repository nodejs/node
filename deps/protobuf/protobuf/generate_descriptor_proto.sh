#!/usr/bin/env bash

# Run this script to regenerate descriptor.pb.{h,cc} after the protocol
# compiler changes.  Since these files are compiled into the protocol compiler
# itself, they cannot be generated automatically by a make rule.  "make check"
# will fail if these files do not match what the protocol compiler would
# generate.
#
# HINT:  Flags passed to generate_descriptor_proto.sh will be passed directly
#   to make when building protoc.  This is particularly useful for passing
#   -j4 to run 4 jobs simultaneously.

if test ! -e src/google/protobuf/stubs/common.h; then
  cat >&2 << __EOF__
Could not find source code.  Make sure you are running this script from the
root of the distribution tree.
__EOF__
  exit 1
fi

cd src

declare -a RUNTIME_PROTO_FILES=(\
  google/protobuf/any.proto \
  google/protobuf/api.proto \
  google/protobuf/descriptor.proto \
  google/protobuf/duration.proto \
  google/protobuf/empty.proto \
  google/protobuf/field_mask.proto \
  google/protobuf/source_context.proto \
  google/protobuf/struct.proto \
  google/protobuf/timestamp.proto \
  google/protobuf/type.proto \
  google/protobuf/wrappers.proto)

declare -a COMPILER_PROTO_FILES=(\
  google/protobuf/compiler/plugin.proto)

CORE_PROTO_IS_CORRECT=0
PROCESS_ROUND=1
BOOTSTRAP_PROTOC=""
while [ $# -gt 0 ]; do
  case $1 in
    --bootstrap_protoc)
      BOOTSTRAP_PROTOC=$2
      shift 2
      ;;
    *)
      break
      ;;
  esac
  shift
done
TMP=$(mktemp -d)
echo "Updating descriptor protos..."
while [ $CORE_PROTO_IS_CORRECT -ne 1 ]
do
  echo "Round $PROCESS_ROUND"
  CORE_PROTO_IS_CORRECT=1

  if [ "$BOOTSTRAP_PROTOC" != "" ]; then
    PROTOC=$BOOTSTRAP_PROTOC
    BOOTSTRAP_PROTOC=""
  else
    make $@ protoc
    if test $? -ne 0; then
      echo "Failed to build protoc."
      exit 1
    fi
    PROTOC="./protoc"
  fi

  $PROTOC --cpp_out=dllexport_decl=PROTOBUF_EXPORT:$TMP ${RUNTIME_PROTO_FILES[@]} && \
  $PROTOC --cpp_out=dllexport_decl=PROTOC_EXPORT:$TMP ${COMPILER_PROTO_FILES[@]}

  for PROTO_FILE in ${RUNTIME_PROTO_FILES[@]} ${COMPILER_PROTO_FILES[@]}; do
    BASE_NAME=${PROTO_FILE%.*}
    diff ${BASE_NAME}.pb.h $TMP/${BASE_NAME}.pb.h > /dev/null
    if test $? -ne 0; then
      CORE_PROTO_IS_CORRECT=0
    fi
    diff ${BASE_NAME}.pb.cc $TMP/${BASE_NAME}.pb.cc > /dev/null
    if test $? -ne 0; then
      CORE_PROTO_IS_CORRECT=0
    fi
  done

  # Only override the output if the files are different to avoid re-compilation
  # of the protoc.
  if [ $CORE_PROTO_IS_CORRECT -ne 1 ]; then
    for PROTO_FILE in ${RUNTIME_PROTO_FILES[@]} ${COMPILER_PROTO_FILES[@]}; do
      BASE_NAME=${PROTO_FILE%.*}
      mv $TMP/${BASE_NAME}.pb.h ${BASE_NAME}.pb.h
      mv $TMP/${BASE_NAME}.pb.cc ${BASE_NAME}.pb.cc
    done
  fi

  PROCESS_ROUND=$((PROCESS_ROUND + 1))
done
cd ..

if test -x objectivec/generate_well_known_types.sh; then
  echo "Generating messages for objc."
  objectivec/generate_well_known_types.sh $@
fi

if test -x csharp/generate_protos.sh; then
  echo "Generating messages for C#."
  csharp/generate_protos.sh $@
fi

if test -x php/generate_descriptor_protos.sh; then
  echo "Generating messages for PHP."
  php/generate_descriptor_protos.sh $@
fi
