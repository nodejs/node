#!/bin/bash
#
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Convenience Script used to rank GC NVP output.

print_usage_and_die() {
  echo "Usage: $0 new-gen-rank|old-gen-rank max|avg logfile"
  exit 1
}

if [ $# -ne 3 ]; then
  print_usage_and_die
fi

case $1 in
  new-gen-rank|old-gen-rank)
    OP=$1
    ;;
  *)
    print_usage_and_die
esac

case $2 in 
  max|avg)
    RANK_MODE=$2
    ;;
  *)
    print_usage_and_die
esac

LOGFILE=$3

GENERAL_INTERESTING_KEYS="\
  pause \
"

INTERESTING_NEW_GEN_KEYS="\
  ${GENERAL_INTERESTING_KEYS} \
  scavenge \
  weak \
  roots \
  old_new \
  code \
  semispace \
  object_groups \
"

INTERESTING_OLD_GEN_KEYS="\
  ${GENERAL_INTERESTING_KEYS} \
  external \
  clear \
  clear.code_flush \
  clear.dependent_code \
  clear.global_handles \
  clear.maps \
  clear.slots_buffer \
  clear.store_buffer \
  clear.string_table \
  clear.weak_cells \
  clear.weak_collections \
  clear.weak_lists \
  finish \
  evacuate \
  evacuate.candidates \
  evacuate.clean_up \
  evacuate.new_space \
  evacuate.update_pointers \
  evacuate.update_pointers.between_evacuated \
  evacuate.update_pointers.to_evacuated \
  evacuate.update_pointers.to_new \
  evacuate.update_pointers.weak \
  mark \
  mark.finish_incremental \
  mark.prepare_code_flush \
  mark.roots \
  mark.weak_closure \
  sweep \
  sweep.code \
  sweep.map \
  sweep.old \
  incremental_finalize \
"

BASE_DIR=$(dirname $0)

case $OP in
  new-gen-rank)
    cat $LOGFILE | grep "gc=s" \
      | $BASE_DIR/eval_gc_nvp.py \
      --no-histogram \
      --rank $RANK_MODE \
      ${INTERESTING_NEW_GEN_KEYS}
    ;;
  old-gen-rank)
    cat $LOGFILE | grep "gc=ms" | grep "reduce_memory=0" | grep -v "steps=0" \
      | $BASE_DIR/eval_gc_nvp.py \
      --no-histogram \
      --rank $RANK_MODE \
      ${INTERESTING_OLD_GEN_KEYS}
    ;;
  *)
    ;;
esac

