#!/bin/bash

set -e

sleep 1  # mtime resolution is 1 sec on unix.
touch "$1"
