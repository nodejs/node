# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [{
    'target_name': 'test',
    'type': 'none',
    'dependencies': ['child_one', 'child_two'],
  }, {
    'target_name': 'child_one',
    'product_name': 'Collide',
    'product_extension': 'bar',
    'sources': ['src.cc'],
    'type': 'loadable_module',
    'mac_bundle': 1,
  }, {
    'target_name': 'child_two',
    'product_name': 'Collide',
    'product_extension': 'foo',
    'sources': ['src.cc'],
    'type': 'loadable_module',
    'mac_bundle': 1,
  }],
}
