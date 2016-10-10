#!/bin/bash

# Add a fixture from the registry

set -e

pkg="$1"
shift
vers="$@"

if [ "$pkg" == "" ]; then
  echo "usage: $0 <pkg> [<versions> ...]" >&2
  exit 1
fi

reg=http://registry.npmjs.org

c () {
  echo "$1 > $2" >&2
  curl -s $1 > $2
}

mkdir -p fixtures/$pkg/-
c $reg/$pkg fixtures/$pkg.json
c $reg/$pkg/latest fixtures/$pkg/latest.json

if [ ${#vers[@]} -eq 0 ]; then
  json=$(cat fixtures/$pkg.json)
  js='console.log(Object.keys('$json'.versions).join(" "))'
  vers=($(node -e "$js"))
fi

for v in "${vers[@]}"; do
  c $reg/$pkg/$v fixtures/$pkg/$v.json
  c $reg/$pkg/-/$pkg-$v.tgz fixtures/$pkg/-/$pkg-$v.tgz
done

find fixtures/$pkg* -type f
