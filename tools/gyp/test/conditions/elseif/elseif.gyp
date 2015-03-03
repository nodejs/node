# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'variables': { 'test_var': 0 },
      'target_name': 'program0',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'includes': [ 'elseif_conditions.gypi' ],
    },
    {
      'variables': { 'test_var': 1 },
      'target_name': 'program1',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'includes': [ 'elseif_conditions.gypi' ],
    },
    {
      'variables': { 'test_var': 2 },
      'target_name': 'program2',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'includes': [ 'elseif_conditions.gypi' ],
    },
    {
      'variables': { 'test_var': 3 },
      'target_name': 'program3',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'includes': [ 'elseif_conditions.gypi' ],
    },
    {
      'variables': { 'test_var': 4 },
      'target_name': 'program4',
      'type': 'executable',
      'sources': [ 'program.cc' ],
      'includes': [ 'elseif_conditions.gypi' ],
    },
  ],
}
