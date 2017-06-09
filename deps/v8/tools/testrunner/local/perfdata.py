# Copyright 2012 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import os
import shelve
import threading


class PerfDataEntry(object):
  def __init__(self):
    self.avg = 0.0
    self.count = 0

  def AddResult(self, result):
    kLearnRateLimiter = 99  # Greater value means slower learning.
    # We use an approximation of the average of the last 100 results here:
    # The existing average is weighted with kLearnRateLimiter (or less
    # if there are fewer data points).
    effective_count = min(self.count, kLearnRateLimiter)
    self.avg = self.avg * effective_count + result
    self.count = effective_count + 1
    self.avg /= self.count


class PerfDataStore(object):
  def __init__(self, datadir, arch, mode):
    filename = os.path.join(datadir, "%s.%s.perfdata" % (arch, mode))
    self.database = shelve.open(filename, protocol=2)
    self.closed = False
    self.lock = threading.Lock()

  def __del__(self):
    self.close()

  def close(self):
    if self.closed: return
    self.database.close()
    self.closed = True

  def GetKey(self, test):
    """Computes the key used to access data for the given testcase."""
    flags = "".join(test.flags)
    return str("%s.%s.%s" % (test.suitename(), test.path, flags))

  def FetchPerfData(self, test):
    """Returns the observed duration for |test| as read from the store."""
    key = self.GetKey(test)
    if key in self.database:
      return self.database[key].avg
    return None

  def UpdatePerfData(self, test):
    """Updates the persisted value in the store with test.duration."""
    testkey = self.GetKey(test)
    self.RawUpdatePerfData(testkey, test.duration)

  def RawUpdatePerfData(self, testkey, duration):
    with self.lock:
      if testkey in self.database:
        entry = self.database[testkey]
      else:
        entry = PerfDataEntry()
      entry.AddResult(duration)
      self.database[testkey] = entry


class PerfDataManager(object):
  def __init__(self, datadir):
    self.datadir = os.path.abspath(datadir)
    if not os.path.exists(self.datadir):
      os.makedirs(self.datadir)
    self.stores = {}  # Keyed by arch, then mode.
    self.closed = False
    self.lock = threading.Lock()

  def __del__(self):
    self.close()

  def close(self):
    if self.closed: return
    for arch in self.stores:
      modes = self.stores[arch]
      for mode in modes:
        store = modes[mode]
        store.close()
    self.closed = True

  def GetStore(self, arch, mode):
    with self.lock:
      if not arch in self.stores:
        self.stores[arch] = {}
      modes = self.stores[arch]
      if not mode in modes:
        modes[mode] = PerfDataStore(self.datadir, arch, mode)
      return modes[mode]


class NullPerfDataStore(object):
  def UpdatePerfData(self, test):
    pass

  def FetchPerfData(self, test):
    return None


class NullPerfDataManager(object):
  def __init__(self):
    pass

  def GetStore(self, *args, **kwargs):
    return NullPerfDataStore()

  def close(self):
    pass


def GetPerfDataManager(context, datadir):
  if context.use_perf_data:
    return PerfDataManager(datadir)
  else:
    return NullPerfDataManager()
