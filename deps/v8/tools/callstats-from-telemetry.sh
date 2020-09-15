#1/bin/env bash
set -e

usage() {
cat << EOF
usage: $0 OPTIONS RESULTS_DIR

Convert telemetry json trace result to callstats.html compatible
versions ot ./out.json

OPTIONS:
  -h           Show this message.
  RESULTS_DIR  tools/perf/artifacts/run_XXX
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

RESULTS_DIR=$1

if [[ ! -e "$RESULTS_DIR" ]]; then
  echo "RESULTS_DIR '$RESULTS_DIR' not found";
  usage;
  exit 1;
fi


OUT=out.json

if [[ -e $OUT ]]; then
  cp --backup=numbered $OUT $OUT.bak
fi


echo '{ "telemetry-results": { "placeholder":{}' > $OUT

for PAGE_DIR in $RESULTS_DIR/*_1; do
  PAGE=`basename $PAGE_DIR`;
  JSON="$PAGE_DIR/trace/traceEvents/*_converted.json";
  du -sh $JSON;
  echo "Converting PAGE=$PAGE";
  echo "," >> $OUT;
  echo "\"$PAGE\": " >> $OUT;
  jq '[.traceEvents[].args | select(."runtime-call-stats" != null) | ."runtime-call-stats"]' $JSON >> $OUT;
done


echo '}}' >> $OUT
