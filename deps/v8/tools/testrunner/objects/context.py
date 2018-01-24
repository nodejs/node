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


class Context():
  def __init__(self, arch, mode, shell_dir, mode_flags, verbose, timeout,
               isolates, command_prefix, extra_flags, noi18n, random_seed,
               no_sorting, rerun_failures_count, rerun_failures_max,
               predictable, no_harness, use_perf_data, sancov_dir):
    self.arch = arch
    self.mode = mode
    self.shell_dir = shell_dir
    self.mode_flags = mode_flags
    self.verbose = verbose
    self.timeout = timeout
    self.isolates = isolates
    self.command_prefix = command_prefix
    self.extra_flags = extra_flags
    self.noi18n = noi18n
    self.random_seed = random_seed
    self.no_sorting = no_sorting
    self.rerun_failures_count = rerun_failures_count
    self.rerun_failures_max = rerun_failures_max
    self.predictable = predictable
    self.no_harness = no_harness
    self.use_perf_data = use_perf_data
    self.sancov_dir = sancov_dir
