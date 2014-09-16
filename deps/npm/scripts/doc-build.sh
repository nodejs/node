#!/usr/bin/env bash

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

if ! [ -x node_modules/.bin/marked ]; then
  ps=0
  if [ -f .building_marked ]; then
    pid=$(cat .building_marked)
    ps=$(ps -p $pid | grep $pid | wc -l) || true
  fi

  if [ -f .building_marked ] && [ $ps != 0 ]; then
    while [ -f .building_marked ]; do
      sleep 1
    done
  else
    # a race to see which make process will be the one to install marked
    echo $$ > .building_marked
    sleep 1
    if [ $(cat .building_marked) == $$ ]; then
      make node_modules/.bin/marked
      rm .building_marked
    else
      while [ -f .building_marked ]; do
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
  *.[1357])
    ./node_modules/.bin/ronn --roff $src \
    | sed "s|@VERSION@|$version|g" \
    | perl -pi -e 's/(npm\\-)?([a-zA-Z\\\.\-]*)\(1\)/npm help \2/g' \
    | perl -pi -e 's/(npm\\-)?([a-zA-Z\\\.\-]*)\(([57])\)/npm help \3 \2/g' \
    | perl -pi -e 's/(npm\\-)?([a-zA-Z\\\.\-]*)\(3\)/npm apihelp \2/g' \
    | perl -pi -e 's/npm\(1\)/npm help npm/g' \
    | perl -pi -e 's/npm\(3\)/npm apihelp npm/g' \
    > $dest
    exit $?
    ;;
  *.html)
    url=${dest/html\//}
    (cat html/dochead.html && \
     cat $src | ./node_modules/.bin/marked &&
     cat html/docfoot.html)\
    | sed "s|@NAME@|$name|g" \
    | sed "s|@DATE@|$date|g" \
    | sed "s|@URL@|$url|g" \
    | sed "s|@VERSION@|$version|g" \
    | perl -pi -e 's/<h1([^>]*)>([^\(]*\([0-9]\)) -- (.*?)<\/h1>/<h1>\2<\/h1> <p>\3<\/p>/g' \
    | perl -pi -e 's/npm-npm/npm/g' \
    | perl -pi -e 's/([^"-])(npm-)?README(?!\.html)(\(1\))?/\1<a href="..\/..\/doc\/README.html">README<\/a>/g' \
    | perl -pi -e 's/<title><a href="[^"]+README.html">README<\/a><\/title>/<title>README<\/title>/g' \
    | perl -pi -e 's/([^"-])([^\(> ]+)(\(1\))/\1<a href="..\/cli\/\2.html">\2\3<\/a>/g' \
    | perl -pi -e 's/([^"-])([^\(> ]+)(\(3\))/\1<a href="..\/api\/\2.html">\2\3<\/a>/g' \
    | perl -pi -e 's/([^"-])([^\(> ]+)(\(5\))/\1<a href="..\/files\/\2.html">\2\3<\/a>/g' \
    | perl -pi -e 's/([^"-])([^\(> ]+)(\(7\))/\1<a href="..\/misc\/\2.html">\2\3<\/a>/g' \
    | perl -pi -e 's/\([1357]\)<\/a><\/h1>/<\/a><\/h1>/g' \
    | (if [ $(basename $(dirname $dest)) == "doc" ]; then
        perl -pi -e 's/ href="\.\.\// href="/g'
      else
        cat
      fi) \
    > $dest
    exit $?
    ;;
  *)
    echo "Invalid destination type: $dest" >&2
    exit 1
    ;;
esac
