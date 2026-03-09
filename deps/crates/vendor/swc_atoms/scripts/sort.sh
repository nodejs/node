#!/usr/bin/env bash
set -eu

cat words.txt | sed '/^[[:space:]]*$/d' | awk '{$1=$1};1' | uniq | ./scripts/_/sort.py > words_sorted.txt
mv words_sorted.txt words.txt