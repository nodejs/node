#!/usr/bin/env python
# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


try:
  from shlex import quote
except ImportError:
  from pipes import quote

try:
  from urllib.request import urlretrieve
except ImportError:
  from urllib import urlretrieve

try:
  xrange = xrange
except NameError:
  xrange = range

try:
  basestring = basestring
except NameError:
  basestring = str

def itervalues(o):
  try:
    return o.itervalues()
  except AttributeError:
    return o.values()


def iteritems(o):
  try:
    return o.iteritems()
  except AttributeError:
    return o.items()
