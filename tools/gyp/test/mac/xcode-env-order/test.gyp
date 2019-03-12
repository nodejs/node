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
        'file.ext1',
        'file.ext2',
        'file.ext3',
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
      # Env vars in actions. The $FOO's are here to test that env vars that
      # aren't defined are handled in some way that doesn't break the build.
      'actions': [
        {
          'action_name': 'Action copy braces ${PRODUCT_NAME} ${FOO}',
          'description': 'Action copy braces ${PRODUCT_NAME} ${FOO}',
          'inputs': [ '${SOURCE_ROOT}/main.c' ],
          # Referencing ${PRODUCT_NAME} in action outputs doesn't work with
          # the Xcode generator (PRODUCT_NAME expands to "Test Support").
          'outputs': [ '<(PRODUCT_DIR)/action-copy-brace.txt' ],
          'action': [ 'cp', '${SOURCE_ROOT}/main.c',
                      '<(PRODUCT_DIR)/action-copy-brace.txt' ],
        },
        {
          'action_name': 'Action copy parens $(PRODUCT_NAME) $(FOO)',
          'description': 'Action copy parens $(PRODUCT_NAME) $(FOO)',
          'inputs': [ '$(SOURCE_ROOT)/main.c' ],
          # Referencing $(PRODUCT_NAME) in action outputs doesn't work with
          # the Xcode generator (PRODUCT_NAME expands to "Test Support").
          'outputs': [ '<(PRODUCT_DIR)/action-copy-paren.txt' ],
          'action': [ 'cp', '$(SOURCE_ROOT)/main.c',
                      '<(PRODUCT_DIR)/action-copy-paren.txt' ],
        },
        {
          'action_name': 'Action copy bare $PRODUCT_NAME $FOO',
          'description': 'Action copy bare $PRODUCT_NAME $FOO',
          'inputs': [ '$SOURCE_ROOT/main.c' ],
          # Referencing $PRODUCT_NAME in action outputs doesn't work with
          # the Xcode generator (PRODUCT_NAME expands to "Test Support").
          'outputs': [ '<(PRODUCT_DIR)/action-copy-bare.txt' ],
          'action': [ 'cp', '$SOURCE_ROOT/main.c',
                      '<(PRODUCT_DIR)/action-copy-bare.txt' ],
        },
      ],
      # Env vars in xcode_settings.
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
      # Env vars in rules. The $FOO's are here to test that env vars that
      # aren't defined are handled in some way that doesn't break the build.
      'rules': [
        {
          'rule_name': 'brace_rule',
          'message': 'Rule braces ${PRODUCT_NAME} ${FOO} <(RULE_INPUT_NAME)',
          'extension': 'ext1',
          'inputs': [ '${SOURCE_ROOT}/main.c' ],
          'outputs': [ '<(PRODUCT_DIR)/rule-copy-brace.txt' ],
          'action': [ 'cp', '${SOURCE_ROOT}/main.c',
                      '<(PRODUCT_DIR)/rule-copy-brace.txt' ],
        },
        {
          'rule_name': 'paren_rule',
          'message': 'Rule parens $(PRODUCT_NAME) $(FOO) <(RULE_INPUT_NAME)',
          'extension': 'ext2',
          'inputs': [ '$(SOURCE_ROOT)/main.c' ],
          'outputs': [ '<(PRODUCT_DIR)/rule-copy-paren.txt' ],
          'action': [ 'cp', '$(SOURCE_ROOT)/main.c',
                      '<(PRODUCT_DIR)/rule-copy-paren.txt' ],
        },
        # TODO: Fails in xcode. Looks like a bug in the xcode generator though
        #       (which uses makefiles for rules, and thinks $PRODUCT_NAME is
        #       $(P)RODUCT_NAME).
        #{
        #  'rule_name': 'bare_rule',
        #  'message': 'Rule copy bare $PRODUCT_NAME $FOO',
        #  'extension': 'ext3',
        #  'inputs': [ '$SOURCE_ROOT/main.c' ],
        #  'outputs': [ '<(PRODUCT_DIR)/rule-copy-bare.txt' ],
        #  'action': [ 'cp', '$SOURCE_ROOT/main.c',
        #              '<(PRODUCT_DIR)/rule-copy-bare.txt' ],
        #},
      ],
    },
  ],
}
