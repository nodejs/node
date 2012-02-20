# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'test_app',
      'product_name': 'Test',
      'type': 'executable',
      'mac_bundle': 1,
      'sources': [
        'main.c',
      ],
      # Env vars in copies.
      'copies': [
        {
          'destination': '<(PRODUCT_DIR)/${PRODUCT_NAME}-copy-brace',
          'files': [ 'main.c', ],  # ${SOURCE_ROOT} doesn't work with xcode
        },
        {
          'destination': '<(PRODUCT_DIR)/$(PRODUCT_NAME)-copy-paren',
          'files': [ '$(SOURCE_ROOT)/main.c', ],
        },
        {
          'destination': '<(PRODUCT_DIR)/$PRODUCT_NAME-copy-bare',
          'files': [ 'main.c', ],  # $SOURCE_ROOT doesn't work with xcode
        },
      ],
      # Env vars in actions.
      'actions': [
        {
          'action_name': 'Action copy braces ${PRODUCT_NAME}',
          'description': 'Action copy braces ${PRODUCT_NAME}',
          'inputs': [ '${SOURCE_ROOT}/main.c' ],
          # Referencing ${PRODUCT_NAME} in action outputs doesn't work with
          # the Xcode generator (PRODUCT_NAME expands to "Test Support").
          'outputs': [ '<(PRODUCT_DIR)/action-copy-brace.txt' ],
          'action': [ 'cp', '${SOURCE_ROOT}/main.c',
                      '<(PRODUCT_DIR)/action-copy-brace.txt' ],
        },
        {
          'action_name': 'Action copy parens ${PRODUCT_NAME}',
          'description': 'Action copy parens ${PRODUCT_NAME}',
          'inputs': [ '${SOURCE_ROOT}/main.c' ],
          # Referencing ${PRODUCT_NAME} in action outputs doesn't work with
          # the Xcode generator (PRODUCT_NAME expands to "Test Support").
          'outputs': [ '<(PRODUCT_DIR)/action-copy-paren.txt' ],
          'action': [ 'cp', '${SOURCE_ROOT}/main.c',
                      '<(PRODUCT_DIR)/action-copy-paren.txt' ],
        },
        {
          'action_name': 'Action copy bare ${PRODUCT_NAME}',
          'description': 'Action copy bare ${PRODUCT_NAME}',
          'inputs': [ '${SOURCE_ROOT}/main.c' ],
          # Referencing ${PRODUCT_NAME} in action outputs doesn't work with
          # the Xcode generator (PRODUCT_NAME expands to "Test Support").
          'outputs': [ '<(PRODUCT_DIR)/action-copy-bare.txt' ],
          'action': [ 'cp', '${SOURCE_ROOT}/main.c',
                      '<(PRODUCT_DIR)/action-copy-bare.txt' ],
        },
      ],
      # Env vars in copies.
      'xcode_settings': {
        'INFOPLIST_FILE': 'Info.plist',
        'STRING_KEY': '/Source/Project',

        'BRACE_DEPENDENT_KEY2': '${STRING_KEY}/${PRODUCT_NAME}',
        'BRACE_DEPENDENT_KEY1': 'D:${BRACE_DEPENDENT_KEY2}',
        'BRACE_DEPENDENT_KEY3': '${PRODUCT_TYPE}:${BRACE_DEPENDENT_KEY1}',

        'PAREN_DEPENDENT_KEY2': '$(STRING_KEY)/$(PRODUCT_NAME)',
        'PAREN_DEPENDENT_KEY1': 'D:$(PAREN_DEPENDENT_KEY2)',
        'PAREN_DEPENDENT_KEY3': '$(PRODUCT_TYPE):$(PAREN_DEPENDENT_KEY1)',

        'BARE_DEPENDENT_KEY2': '$STRING_KEY/$PRODUCT_NAME',
        'BARE_DEPENDENT_KEY1': 'D:$BARE_DEPENDENT_KEY2',
        'BARE_DEPENDENT_KEY3': '$PRODUCT_TYPE:$BARE_DEPENDENT_KEY1',

        'MIXED_DEPENDENT_KEY': '${STRING_KEY}:$(PRODUCT_NAME):$MACH_O_TYPE',
      },
    },
  ],
}
