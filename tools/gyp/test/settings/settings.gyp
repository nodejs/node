# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'settings_target',
      'type': 'settings',
      # In real life, this would set 'cflags' etc and other targets
      # would depend on it.
    },
    {
      # This is needed so scons will actually generate a SConstruct
      # (which it doesn't do for settings targets alone).
      'target_name': 'junk1',
      'type': 'none',
    },
  ],
}
