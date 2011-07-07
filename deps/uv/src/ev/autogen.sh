#!/bin/sh

libtoolize --force
automake --add-missing --force-missing
autoreconf

