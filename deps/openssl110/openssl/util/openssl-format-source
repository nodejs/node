#!/bin/sh
#
# Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

#
# openssl-format-source 
# - format source tree according to OpenSSL coding style using indent
#
# usage:
#   openssl-format-source [-v] [-n] [file|directory] ...
#
# note: the indent options assume GNU indent v2.2.10 which was released
#       Feb-2009 so if you have an older indent the options may not 
#	match what is expected
#
# any marked block comment blocks have to be moved to align manually after
# the reformatting has been completed as marking a block causes indent to 
# not move it at all ...
#

PATH=/usr/local/bin:/bin:/usr/bin:$PATH
export PATH
HERE="`dirname $0`"

set -e

INDENT=indent
uname -s | grep BSD > /dev/null && type gindent > /dev/null 2>&1 && INDENT=gindent

if [ $# -eq 0 ]; then
  echo "usage: $0 [-v] [-n] [-c] [sourcefile|sourcedir] ..." >&2
  exit 1
fi

VERBOSE=false
DONT=false
STOPARGS=false
COMMENTS=false
CHANGED=false
DEBUG=""

# for this exercise, we want to force the openssl style, so we roll
# our own indent profile, which is at a well known location
INDENT_PROFILE="$HERE/indent.pro"
export INDENT_PROFILE
if [ ! -f "$INDENT_PROFILE" ]; then
  echo "$0: unable to locate the openssl indent.pro file" >&2
  exit 1
fi

# Extra arguments; for adding the comment-formatting
INDENT_ARGS=""
for i 
do
  if [ "$STOPARGS" != "true" ]; then
    case $i in
      --) STOPARGS="true"; continue;;
      -n) DONT="true"; continue;;
      -v) VERBOSE="true"; 
	  echo "INDENT_PROFILE=$INDENT_PROFILE";
	  continue;;
      -c) COMMENTS="true"; 
      	  INDENT_ARGS="-fc1 -fca -cdb -sc"; 
	  continue;;
      -nc) COMMENTS="true";
	  continue;;
      -d) DEBUG='eval tee "$j.pre" |'
	  continue;;
    esac
  fi

  if [ -d "$i" ]; then
    LIST=`find "$i" -name '*.[ch]' -print`
  else 
    if [ ! -f "$i" ]; then
      echo "$0: source file not found: $i" >&2
      exit 1
    fi
    LIST="$i"
  fi
  
  for j in $LIST
  do
    # ignore symlinks - we only ever process the base file - so if we
    # expand a directory tree we need to ignore any located symlinks
    if [ -d "$i" ]; then
      if [ -h "$j" ]; then
	continue;
      fi
    fi

    if [ "$DONT" = "false" ]; then
      tmp=$(mktemp /tmp/indent.XXXXXX)
      trap 'rm -f "$tmp"' HUP INT TERM EXIT

      case `basename $j` in 
	# the list of files that indent is unable to handle correctly
	# that we simply leave alone for manual formatting now
	obj_dat.h|aes_core.c|aes_x86core.c|ecp_nistz256.c)
	  echo "skipping $j"
	  ;;
	*)
	  if [ "$COMMENTS" = "true" ]; then
	    # we have to mark single line comments as /*- ...*/ to stop indent
	    # messing with them, run expand then indent as usual but with the
	    # the process-comments options and then undo that marking, and then 
	    # finally re-run indent without process-comments so the marked-to-
	    # be-ignored comments we did automatically end up getting moved 
	    # into the right possition within the code as indent leaves marked 
	    # comments entirely untouched - we appear to have no way to avoid 
	    # the double processing and get the desired output
	    cat "$j" | \
	    expand | \
	    perl -0 -np \
	      -e 's/(\n#[ \t]*ifdef[ \t]+__cplusplus\n[^\n]*\n#[ \t]*endif\n)/\n\/**INDENT-OFF**\/$1\/**INDENT-ON**\/\n/g;' \
	      -e 's/(\n\/\*\!)/\n\/**/g;' \
	      -e 's/(STACK_OF|LHASH_OF)\(([^ \t,\)]+)\)( |\n)/$1_$2_$3/g;' \
	      | \
	    perl -np \
	      -e 's/^([ \t]*)\/\*([ \t]+.*)\*\/[ \t]*$/my ($x1,$x2) = ($1, $2); if (length("$x1$x2")<75 && $x2 !~ m#^\s*\*INDENT-(ON|OFF)\*\s*$#) {$c="-"}else{$c=""}; "$x1\/*$c$x2*\/"/e;' \
	      -e 's/^\/\* ((Copyright|=|----).*)$/\/*-$1/;' \
	      -e 's/^((DECLARE|IMPLEMENT)_.*)$/\/**INDENT-OFF**\/\n$1\n\/**INDENT-ON**\//;' \
	      -e 's/^([ \t]*(make_dh|make_dh_bn|make_rfc5114_td)\(.*\)[ \t,]*)$/\/**INDENT-OFF**\/\n$1\n\/**INDENT-ON**\//;' \
	      -e 's/^(ASN1_ADB_TEMPLATE\(.*)$/\/**INDENT-OFF**\/\n$1\n\/**INDENT-ON**\//;' \
	      -e 's/^((ASN1|ADB)_.*_(end|END)\(.*[\){=,;]+[ \t]*)$/$1\n\/**INDENT-ON**\//;' \
	      -e '/ASN1_(ITEM_ref|ITEM_ptr|ITEM_rptr|PCTX)/ || s/^((ASN1|ADB)_[^\*]*[){=,]+[ \t]*)$/\/**INDENT-OFF**\/\n$1/;' \
	      -e 's/^(} (ASN1|ADB)_[^\*]*[\){=,;]+)$/$1\n\/**INDENT-ON**\//;' \
	      | \
	      $DEBUG $INDENT $INDENT_ARGS | \
	      perl -np \
		-e 's/^([ \t]*)\/\*-(.*)\*\/[ \t]*$/$1\/*$2*\//;' \
		-e 's/^\/\*-((Copyright|=|----).*)$/\/* $1/;' \
	      | $INDENT | \
	      perl -0 -np \
		-e 's/\/\*\*INDENT-(ON|OFF)\*\*\/\n//g;' \
	      | perl -np \
	        -e 's/(STACK_OF|LHASH_OF)_([^ \t,]+)_( |\/)/$1($2)$3/g;' \
	        -e 's/(STACK_OF|LHASH_OF)_([^ \t,]+)_$/$1($2)/g;' \
	      | perl "$HERE"/su-filter.pl \
	      > "$tmp"
	  else
	    expand "$j" | $INDENT $INDENT_ARGS > "$tmp"
	  fi;
	  if cmp -s "$tmp" "$j"; then
	    if [ "$VERBOSE" = "true" ]; then
	      echo "$j unchanged"
	    fi
	    rm "$tmp"
	  else
	    if [ "$VERBOSE" = "true" ]; then
	      echo "$j changed"
	    fi
	    CHANGED=true
	    mv "$tmp" "$j"
	  fi
	  ;;
      esac
    fi
  done
done


if [ "$VERBOSE" = "true" ]; then
  echo
  if [ "$CHANGED" = "true" ]; then
    echo "SOURCE WAS MODIFIED"
  else
    echo "SOURCE WAS NOT MODIFIED"
  fi
fi
