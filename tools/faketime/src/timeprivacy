#!/bin/bash

## Copyright (c) 2013, adrelanos at riseup dot net
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met: 
##
## 1. Redistributions of source code must retain the above copyright notice, this
## list of conditions and the following disclaimer. 
## 2. Redistributions in binary form must reproduce the above copyright notice,
## this list of conditions and the following disclaimer in the documentation
## and/or other materials provided with the distribution. 
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
## ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
## WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
## DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
## ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
## (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
## LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
## ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
## SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#set -x

SCRIPTNAME="$(basename $0)"

usage() {
   echo "$SCRIPTNAME

Usage: $SCRIPTNAME [-h help] [-d day] [-m month] [-y year] [-i increment in seconds (0-60)] [-r random increment in seconds (0-60)] [-f history folder]
Example: $SCRIPTNAME -d 30 -m 12 -y 2013 -i 10 -f /tmp/$SCRIPTNAMEtest
         sudo $SCRIPTNAME -d 30 -m 12 -y 2013 -r -f /tmp/$SCRIPTNAMEtest"
}

_randomincrement="none"
_increment="none"

while [ -n "$1" ]; do
  case "$1" in
      -h)
          usage
          exit 0
          ;;
      -d)
          _day="$2"
          shift
          ;;
      -m)
          _month="$2"
          shift
          ;;
      -y)
          _year="$2"
          shift
          ;;
      -i)
          _increment="$2"
          shift
          ;;
      -r)
          _randomincrement="$2"
          shift
          ;;
      -f)
          TIMEDIR="$2"
          shift
          ;;
      *)
          command="$(which $1)"
          ## From now on the complete to-be wrapped command + its args
          ## are stored in $@, which will expand like we want it for
          ## handling quoted arguments with whitespaces in it, etc.
          break
  esac
  shift
done

if [ -z "$_day" ]; then
   _day="$(date +"%d")"
fi

if [ -z "$_month" ]; then
   _month="$(date +"%m")"
fi

if [ -z "$_year" ]; then
   _year="$(date +"%Y")"
fi

if [ "$_randomincrement" = "none" ] && [ "$_increment" = "none" ]; then
   _increment="1"
fi

if [ "$_randomincrement" = "none" ]; then
   if [ -z "$_increment" ]; then
      _increment="1"
   fi
elif [ "$_increment" = "none" ]; then
   if [ "$_randomincrement" = "" ]; then
      echo "randomincrement must be a positive number."
      exit 1
   else
      ## random number between 1 and $_randomincrement
      random_number="$(( 0+($(od -An -N2 -i /dev/random) )%($_randomincrement-0+1) ))"
      _increment="$random_number"
   fi
else
   echo "You can not combine -r and -i."
   exit 1
fi

if [ -z "$TIMEDIR" ]; then
   TIMEDIR=~/.timeprivacy
fi

nodigits="$(echo $_increment | sed 's/[[:digit:]]//g')"
if [ ! -z "$nodigits" ]; then
   echo "increment is not a digit."
   exit 1
fi

nodigits="$(echo $_year | sed 's/[[:digit:]]//g')"
if [ ! -z "$nodigits" ]; then
   echo "_day is not a digit."
   exit 1
fi

nodigits="$(echo $_year | sed 's/[[:digit:]]//g')"
if [ ! -z "$nodigits" ]; then
   echo "year is not a digit."
   exit 1
fi

nodigits="$(echo $_month | sed 's/[[:digit:]]//g')"
if [ ! -z "$nodigits" ]; then
   echo "month is not a digit."
   exit 1
fi

nodigits="$(echo $_day | sed 's/[[:digit:]]//g')"
if [ ! -z "$nodigits" ]; then
   echo "day is not a digit."
   exit 1
fi

nodigits="$(echo $_increment | sed 's/[[:digit:]]//g')"
if [ ! -z "$nodigits" ]; then
   echo "increment is not a digit."
   exit 1
fi

SECONDS_FILE="$TIMEDIR/seconds_file"
MINUTES_FILE="$TIMEDIR/minutes_file"
HOURS_FILE="$TIMEDIR/hours_file"

#DAYS_FILE="$TIMEDIR/days_file"
#MONTHS_FILE="$TIMEDIR/months_file"
#YEARS_FILE="$TIMEDIR/years_file"

#true "TIMEDIR: $TIMEDIR"
#true "year: $_year"
#true "month: $_month"
#true "day: $_day"
#true "_randomincrement: $_randomincrement"
#true "_increment: $_increment"

read_date_file() {
   if [ ! -d "$TIMEDIR" ]; then
      mkdir -p "$TIMEDIR"
   fi

   if [ ! -f "$SECONDS_FILE" ]; then
      echo "0" > "$SECONDS_FILE"
   fi

   if [ ! -f "$MINUTES_FILE" ]; then
      echo "0" > "$MINUTES_FILE"
   fi

   if [ ! -f "$HOURS_FILE" ]; then
      echo "0" > "$HOURS_FILE"
   fi

   #if [ ! -f "$DAYS_FILE" ]; then
      #echo "1" > "$DAYS_FILE"
   #fi

   #if [ ! -f "$MONTHS_FILE" ]; then
      #echo "1" > "$MONTHS_FILE"
   #fi

   #if [ ! -f "$YEARS_FILE" ]; then
      #echo "2013" > "$YEARS_FILE"
   #fi

   SECONDS="$(cat "$SECONDS_FILE")"
   MINUTES="$(cat "$MINUTES_FILE")"
   HOURS="$(cat "$HOURS_FILE")"

   if [ -z "$SECONDS" ]; then
      SECONDS="0"
   fi

   if [ -z "$MINUTES" ]; then
      MINUTES="0"
   fi

   if [ -z "$HOURS" ]; then
      HOURS="0"
   fi

   local nodigits="$(echo $SECONDS | sed 's/[[:digit:]]//g')"
   if [ ! -z "$nodigits" ]; then
      SECONDS="0"
   fi

   local nodigits="$(echo $MINUTES | sed 's/[[:digit:]]//g')"
   if [ ! -z "$nodigits" ]; then
      MINUTES="0"
   fi

   local nodigits="$(echo $HOURS | sed 's/[[:digit:]]//g')"
   if [ ! -z "$nodigits" ]; then
      HOURS="0"
   fi

   SECONDS="$(expr "$SECONDS" + "$_increment")" || true
   if [ "$SECONDS" -ge "60" ]; then
      SECONDS="0"

      MINUTES="$(expr "$MINUTES" + "1")" || true
      if [ "$MINUTES" -ge "60" ]; then
         MINUTES="0"

         HOURS="$(expr "$HOURS" + "1")" || true
         if [ "$HOURS" -ge "24" ]; then
            HOURS="0"
         fi
         echo "$HOURS" > "$HOURS_FILE"

      fi
      echo "$MINUTES" > "$MINUTES_FILE"

   fi

   echo "$SECONDS" > "$SECONDS_FILE"

   #echo "$HOURS $MINUTES $SECONDS"
}

need_new_date() {
   ## Testing
   #while [ 1 ]; do
   #   read_date_file
   #done

   read_date_file

   ## Testing
   #echo "faketime '$_year-$_month-$_day $HOURS:$MINUTES:$SECONDS' /bin/date"
   #faketime "$_year-$_month-$_day $HOURS:$MINUTES:$SECONDS" /bin/date

   echo "$_year-$_month-$_day $HOURS:$MINUTES:$SECONDS"
}

need_new_date

