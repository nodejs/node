#!/usr/bin/env python
#
# Copyright 2013 the V8 project authors. All rights reserved.
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

# This is a utility for populating the lookup table for the
# approximation of trigonometric functions.

import sys, math

SAMPLES = 1800

TEMPLATE = """\
// Copyright 2013 Google Inc. All Rights Reserved.

// This file was generated from a python script.

#include "v8.h"
#include "trig-table.h"

namespace v8 {
namespace internal {

  const double TrigonometricLookupTable::kSinTable[] =
      { %(sine_table)s };
  const double TrigonometricLookupTable::kCosXIntervalTable[] =
      { %(cosine_table)s };
  const int TrigonometricLookupTable::kSamples = %(samples)i;
  const int TrigonometricLookupTable::kTableSize = %(table_size)i;
  const double TrigonometricLookupTable::kSamplesOverPiHalf =
      %(samples_over_pi_half)s;

} }  // v8::internal
"""

def main():
  pi_half = math.pi / 2
  interval = pi_half / SAMPLES
  sin = []
  cos_times_interval = []
  table_size = SAMPLES + 2

  for i in range(0, table_size):
    sample = i * interval
    sin.append(repr(math.sin(sample)))
    cos_times_interval.append(repr(math.cos(sample) * interval))

  output_file = sys.argv[1]
  output = open(str(output_file), "w")
  output.write(TEMPLATE % {
    'sine_table': ','.join(sin),
    'cosine_table': ','.join(cos_times_interval),
    'samples': SAMPLES,
    'table_size': table_size,
    'samples_over_pi_half': repr(SAMPLES / pi_half)
  })

if __name__ == "__main__":
  main()
