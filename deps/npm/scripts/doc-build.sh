#!/bin/bash

if [[ $DEBUG != "" ]]; then
  set -x
fi
set -o errexit
set -o pipefail

if ! [ -x node_modules/.bin/ronn ]; then
  ps=0
  if [ -f .building_ronn ]; then
    pid=$(cat .building_ronn)
    ps=$(ps -p $pid | grep $pid | wc -l) || true
  fi

  if [ -f .building_ronn ] && [ $ps != 0 ]; then
    while [ -f .building_ronn ]; do
      sleep 1
    done
  else
    # a race to see which make process will be the one to install ronn
    echo $$ > .building_ronn
    sleep 1
    if [ $(cat .building_ronn) == $$ ]; then
      make node_modules/.bin/ronn
      rm .building_ronn
    else
      while [ -f .building_ronn ]; do
        sleep 1
      done
    fi
  fi
fi

src=$1
dest=$2
name=$(basename ${src%.*})
date=$(date -u +'%Y-%M-%d %H:%m:%S')
version=$(node cli.js -v)

mkdir -p $(dirname $dest)

case $dest in
  *.[13])
    ./node_modules/.bin/ronn --roff $src \
    | sed "s|@VERSION@|$version|g" \
    | perl -pi -e 's/npm\\-([^\(]*)\(1\)/npm help \1/g' \
    | perl -pi -e 's/npm\\-([^\(]*)\(3\)/npm apihelp \1/g' \
    | perl -pi -e 's/npm\(1\)/npm help npm/g' \
    | perl -pi -e 's/npm\(3\)/npm apihelp npm/g' \
    > $dest
    exit $?
    ;;
  *.html)
    (cat html/dochead.html && \
     ./node_modules/.bin/ronn -f $src && \
     cat html/docfoot.html )\
    | sed "s|@NAME@|$name|g" \
    | sed "s|@DATE@|$date|g" \
    | sed "s|@VERSION@|$version|g" \
    | perl -pi -e 's/<h1>npm(-?[^\(]*\([0-9]\)) -- (.*?)<\/h1>/<h1>npm\1<\/h1> <p>\2<\/p>/g' \
    | perl -pi -e 's/npm-npm/npm/g' \
    | perl -pi -e 's/([^"-])(npm-)?README(\(1\))?/\1<a href="..\/doc\/README.html">README<\/a>/g' \
    | perl -pi -e 's/<title><a href="..\/doc\/README.html">README<\/a><\/title>/<title>README<\/title>/g' \
    | perl -pi -e 's/([^"-])npm-([^\(]+)(\(1\))/\1<a href="..\/doc\/\2.html">\2\3<\/a>/g' \
    | perl -pi -e 's/([^"-])npm-([^\(]+)(\(3\))/\1<a href="..\/api\/\2.html">\2\3<\/a>/g' \
    | perl -pi -e 's/([^"-])npm\(1\)/\1<a href="..\/doc\/npm.html">npm(1)<\/a>/g' \
    | perl -pi -e 's/([^"-])npm\(3\)/\1<a href="..\/api\/npm.html">npm(3)<\/a>/g' \
    | perl -pi -e 's/\([13]\)<\/a><\/h1>/<\/a><\/h1>/g' \
    > $dest
    exit $?
    ;;
  *)
    echo "Invalid destination type: $dest" >&2
    exit 1
    ;;
esac
