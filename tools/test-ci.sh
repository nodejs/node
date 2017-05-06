#!/bin/bash
cd `dirname $0`/.. > /dev/null
`which python` tools/test.py -J --mode=release $*
