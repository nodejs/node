#!/bin/bash

# ensure that we get the npm being tested, not some global thing.
npmbin=$npm_config_prefix/bin/npm
npm () {
  node $npmbin "$@"
}

# work around the weird env we're in, as part of npm's test
export npm_config_prefix=$PWD
unset npm_config_global
unset npm_config_depth

out=$(diff <(npm ls --json) npm-shrinkwrap.json)
if [ "$out" != "" ]; then
  echo "Didn't get expected packages" >&2
  echo "$out" >&2
  exit 1
fi
echo "ok"
