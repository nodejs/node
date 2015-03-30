# Copyright (c) 2014 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'do_actions',
      'type': 'none',
      'actions': [{
        'action_name': 'should_be_forbidden',
        'inputs': [ 'name with spaces' ],
        'outputs': [ '<(SHARED_INTERMEDIATE_DIR)/name with spaces' ],
        'action': [ 'true' ],
      }],
    },
  ],
}
