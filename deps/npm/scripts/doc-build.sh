#!/usr/bin/env bash

if [[ $DEBUG != "" ]]; then
  set -x
fi
set -o errexit
set -o pipefail

src=$1
dest=$2
name=$(basename ${src%.*})
date=$(date -u +'%Y-%m-%d %H:%M:%S')
version=$(node bin/npm-cli.js -v)

mkdir -p $(dirname $dest)

html_replace_tokens () {
	local url=$1
	sed "s|@NAME@|$name|g" \
	| sed "s|@DATE@|$date|g" \
	| sed "s|@URL@|$url|g" \
	| sed "s|@VERSION@|$version|g" \
	| perl -p -e 's/<h1([^>]*)>([^\(]*\([0-9]\)) -- (.*?)<\/h1>/<h1>\2<\/h1> <p>\3<\/p>/g' \
	| perl -p -e 's/npm-npm/npm/g' \
	| perl -p -e 's/([^"-])(npm-)?README(?!\.html)(\(1\))?/\1<a href="..\/..\/doc\/README.html">README<\/a>/g' \
	| perl -p -e 's/<title><a href="[^"]+README.html">README<\/a><\/title>/<title>README<\/title>/g' \
	| perl -p -e 's/([^"-])([^\(> ]+)(\(1\))/\1<a href="..\/cli\/\2.html">\2\3<\/a>/g' \
	| perl -p -e 's/([^"-])([^\(> ]+)(\(3\))/\1<a href="..\/api\/\2.html">\2\3<\/a>/g' \
	| perl -p -e 's/([^"-])([^\(> ]+)(\(5\))/\1<a href="..\/files\/\2.html">\2\3<\/a>/g' \
	| perl -p -e 's/([^"-])([^\(> ]+)(\(7\))/\1<a href="..\/misc\/\2.html">\2\3<\/a>/g' \
	| perl -p -e 's/\([1357]\)<\/a><\/h1>/<\/a><\/h1>/g' \
	| (if [ $(basename $(dirname $dest)) == "doc" ]; then
			perl -p -e 's/ href="\.\.\// href="/g'
		else
			cat
		fi)
}

man_replace_tokens () {
	sed "s|@VERSION@|$version|g" \
	| perl -p -e 's/(npm\\-)?([a-zA-Z\\\.\-]*)\(1\)/npm help \2/g' \
	| perl -p -e 's/(npm\\-)?([a-zA-Z\\\.\-]*)\(([57])\)/npm help \3 \2/g' \
	| perl -p -e 's/(npm\\-)?([a-zA-Z\\\.\-]*)\(3\)/npm apihelp \2/g' \
	| perl -p -e 's/npm\(1\)/npm help npm/g' \
	| perl -p -e 's/npm\(3\)/npm apihelp npm/g'
}

case $dest in
  *.[1357])
    ./node_modules/.bin/marked-man --roff $src \
    | man_replace_tokens > $dest
    exit $?
    ;;
  *.html)
    url=${dest/html\//}
    (cat html/dochead.html && \
     cat $src | ./node_modules/.bin/marked &&
     cat html/docfoot.html)\
    | html_replace_tokens $url \
    > $dest
    exit $?
    ;;
  *)
    echo "Invalid destination type: $dest" >&2
    exit 1
    ;;
esac
