#!/bin/bash
#
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Convenience Script used to rank GC NVP output.

print_usage_and_die() {
  echo "Usage: $0 [OPTIONS]"
  echo ""
  echo "OPTIONS"
  echo  "  -r|--rank new-gen-rank|old-gen-rank    GC mode to profile"
  echo  "                                         (default: old-gen-rank)"
  echo  "  -s|--sort avg|max                      sorting mode (default: max)"
  echo  "  -t|--top-level                         include top-level categories"
  echo  "  -c|--csv                               provide csv output"
  echo  "  -f|--file FILE                         profile input in a file"
  echo  "                                         (default: stdin)"
  echo  "  -p|--percentiles                       comma separated percentiles"
  exit 1
}

OP=old-gen-rank
RANK_MODE=max
TOP_LEVEL=no
CSV=""
LOGFILE=/dev/stdin
PERCENTILES=""

while [[ $# -ge 1 ]]
do
  key="$1"
  case $key in
    -r|--rank)
      case $2 in
        new-gen-rank|old-gen-rank)
          OP="$2"
          ;;
        *)
          print_usage_and_die
      esac
      shift
      ;;
    -s|--sort)
      case $2 in
        max|avg)
          RANK_MODE=$2
          ;;
        *)
          print_usage_and_die
      esac
      shift
      ;;
    -t|--top-level)
      TOP_LEVEL=yes
      ;;
    -c|--csv)
      CSV=" --csv "
      ;;
    -f|--file)
      LOGFILE=$2
      shift
      ;;
    -p|--percentiles)
      PERCENTILES="--percentiles=$2"
      shift
      ;;
    *)
      break
      ;;
  esac
  shift
done

if [[ $# -ne 0 ]]; then
  echo "Unknown option(s): $@"
  echo ""
  print_usage_and_die
fi

INTERESTING_NEW_GEN_KEYS="\
  scavenge \
  weak \
  roots \
  old_new \
  semispace \
"

INTERESTING_OLD_GEN_KEYS="\
  clear.dependent_code \
  clear.global_handles \
  clear.maps \
  clear.slots_buffer \
  clear.store_buffer \
  clear.string_table \
  clear.weak_cells \
  clear.weak_collections \
  clear.weak_lists \
  evacuate.candidates \
  evacuate.clean_up \
  evacuate.copy \
  evacuate.update_pointers \
  evacuate.update_pointers.to_evacuated \
  evacuate.update_pointers.to_new \
  evacuate.update_pointers.weak \
  external.mc_prologue \
  external.mc_epilogue \
  external.mc_incremental_prologue \
  external.mc_incremental_epilogue \
  external.weak_global_handles \
  mark.finish_incremental \
  mark.roots \
  mark.weak_closure \
  mark.weak_closure.ephemeral \
  mark.weak_closure.weak_handles \
  mark.weak_closure.weak_roots \
  mark.weak_closure.harmony \
  sweep.code \
  sweep.map \
  sweep.old \
"

if [[ "$TOP_LEVEL" = "yes" ]]; then
  INTERESTING_OLD_GEN_KEYS="\
    ${INTERESTING_OLD_GEN_KEYS} \
    clear \
    evacuate \
    finish \
    incremental_finalize \
    mark \
    pause
    sweep \
  "
  INTERESTING_NEW_GEN_KEYS="\
    ${INTERESTING_NEW_GEN_KEYS} \
  "
fi

BASE_DIR=$(dirname $0)

case $OP in
  new-gen-rank)
    cat $LOGFILE | grep "gc=s" \
      | $BASE_DIR/eval_gc_nvp.py \
      --no-histogram \
      --rank $RANK_MODE \
      $CSV \
      $PERCENTILES \
      ${INTERESTING_NEW_GEN_KEYS}
    ;;
  old-gen-rank)
    cat $LOGFILE | grep "gc=ms" \
      | $BASE_DIR/eval_gc_nvp.py \
      --no-histogram \
      --rank $RANK_MODE \
      $CSV \
      $PERCENTILES \
      ${INTERESTING_OLD_GEN_KEYS}
    ;;
  *)
    ;;
esac
