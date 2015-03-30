#!/usr/bin/env python

# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that it's possible to build host targets correctly in a multilib
configuration, explicitly forcing either 32 or 64 bit.
"""

import TestGyp

test = TestGyp.TestGyp(formats=['android'])

test.run_gyp('host_32or64.gyp')

# Force building as 32-bit
test.build('host_32or64.gyp', 'generate_output',
           arguments=['GYP_HOST_VAR_PREFIX=$(HOST_2ND_ARCH_VAR_PREFIX)',
                      'GYP_HOST_MULTILIB=32'])

test.built_file_must_match('host_32or64.output', 'Hello, 32-bit world!\n')

# Force building as 64-bit
test.build('host_32or64.gyp', 'generate_output',
           arguments=['GYP_HOST_VAR_PREFIX=',
                      'GYP_HOST_MULTILIB=64'])

test.built_file_must_match('host_32or64.output', 'Hello, 64-bit world!\n')

test.pass_test()
