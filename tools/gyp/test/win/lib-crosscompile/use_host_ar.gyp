# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
 'targets': [
    {
      'target_name': 'lib_answer',
      'type': 'static_library',
      'toolsets': ['host'],
      'msvs_settings': {
        'msvs_cygwin_shell': 0,
      },
      'sources': ['answer.cc'],
    },
  ]
}
