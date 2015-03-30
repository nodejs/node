# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# dep.gyp contains a target dep, on which all the targets in the project
# depend. This means there's a self-dependency of dep on itself, which is
# pruned by setting prune_self_dependency to 1.

{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'dep',
      'type': 'none',
      'variables': {
        # Without this GYP will report a cycle in dependency graph.
        'prune_self_dependency': 1,
      },
    },
  ],
}
