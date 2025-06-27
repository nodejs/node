#1/bin/env bash
set -e

usage() {
cat << EOF
usage: $0 OPTIONS RESULTS_DIR | TRACE_JSON

Convert telemetry json trace result to callstats.html compatible
versions ot ./out.json

OPTIONS:
  -h           Show this message.
  RESULTS_DIR  tools/perf/artifacts/run_XXX
  TRACE_JSON   .json trace files
EOF
}


while getopts ":h" OPTION ; do
  case $OPTION in
    h)  usage
        exit 0
        ;;
    ?)  echo "Illegal option: -$OPTARG"
        usage
        exit 1
        ;;
  esac
done

# =======================================================================

if [[ "$1" == *.json ]]; then
  echo "Converting json files"
  JSON=$1
  OUT="${JSON%.json}.rcs.json"
elif [[ -e "$1" ]]; then
  echo "Converting results dir"
  RESULTS_DIR=$1
  OUT="${RESULTS_DIR}/combined.rcs.json"
else
  echo "RESULTS_DIR '$RESULTS_DIR' not found";
  usage;
  exit 1;
fi


if [[ -e $OUT ]]; then
  echo "# Creating backup for $OUT"
  cp $OUT $OUT.bak
fi
echo "# Writing to $OUT"


function convert {
  NAME=$1
  JSON=$2
  # Check if any json file exists:
  if ls $JSON 1> /dev/null 2>&1; then
    du -sh $JSON;
    echo "Converting NAME=$NAME";
    echo "," >> $OUT;
    echo "\"$NAME\": " >> $OUT;
    jq '[.traceEvents[].args | select(."runtime-call-stats" != null) | ."runtime-call-stats"]' $JSON >> $OUT;
  fi
}


echo '{ "telemetry-results": { "placeholder":{}' > $OUT
if [[ $RESULTS_DIR ]]; then
  for PAGE_DIR in $RESULTS_DIR/*_1; do
    NAME=`basename $PAGE_DIR`;
    JSON="$PAGE_DIR/trace/traceEvents/*_converted.json";
    convert $NAME $JSON
  done
else
  for JSON in $@; do
    convert $JSON $JSON
  done
fi
echo '}}' >> $OUT


echo ""
echo "RESULT: ${OUT}"
