# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a simple test file to make sure that variable substitution
# happens correctly.  Run "run_tests.py" using python to generate the
# output from this gyp file.

{
  'variables': {
    'pi': 'import math; print math.pi',
    'third_letters': "<(other_letters)HIJK",
    'letters_list': 'ABCD',
    'other_letters': '<(letters_list)EFG',
    'check_included': '<(included_variable)',
    'check_lists': [
      '<(included_variable)',
      '<(third_letters)',
    ],
    'check_int': 5,
    'check_str_int': '6',
    'check_list_int': [
      7,
      '8',
      9,
    ],
    'not_int_1': ' 10',
    'not_int_2': '11 ',
    'not_int_3': '012',
    'not_int_4': '13.0',
    'not_int_5': '+14',
    'negative_int': '-15',
    'zero_int': '0',
  },
  'includes': [
    'commands.gypi',
  ],
  'targets': [
    {
      'target_name': 'foo',
      'type': 'none',
      'variables': {
        'var1': '<!(["python", "-c", "<(pi)"])',
        'var2': '<!(python -c "print \'<!(python -c "<(pi)") <(letters_list)\'")',
        'var3': '<!(python -c "print \'<(letters_list)\'")',
        'var4': '<(<!(python -c "print \'letters_list\'"))',
        'var5': 'letters_',
        'var6': 'list',
        'var7': '<(check_int)',
        'var8': '<(check_int)blah',
        'var9': '<(check_str_int)',
        'var10': '<(check_list_int)',
        'var11': ['<@(check_list_int)'],
        'var12': '<(not_int_1)',
        'var13': '<(not_int_2)',
        'var14': '<(not_int_3)',
        'var15': '<(not_int_4)',
        'var16': '<(not_int_5)',
        'var17': '<(negative_int)',
        'var18': '<(zero_int)',
        # A second set with different names to make sure they only execute the
        # commands once.
        'var1prime': '<!(["python", "-c", "<(pi)"])',
        'var2prime': '<!(python -c "print \'<!(python -c "<(pi)") <(letters_list)\'")',
        'var3prime': '<!(python -c "print \'<(letters_list)\'")',
        'var4prime': '<(<!(python -c "print \'letters_list\'"))',
      },
      'actions': [
        {
          'action_name': 'test_action',
          'variables': {
            'var7': '<!(echo <(var5)<(var6))',
          },
          'inputs' : [
            '<(var2)',
          ],
          'outputs': [
            '<(var4)',
            '<(var7)',
          ],
          'action': [
            'echo',
            '<(_inputs)',
            '<(_outputs)',
          ],
        },
        # Again with the same vars to make sure the right things happened.
        {
          'action_name': 'test_action_prime',
          'variables': {
            'var7': '<!(echo <(var5)<(var6))',
          },
          'inputs' : [
            '<(var2)',
          ],
          'outputs': [
            '<(var4)',
            '<(var7)',
          ],
          'action': [
            'echo',
            '<(_inputs)',
            '<(_outputs)',
          ],
        },
        # And one more time with the other vars...
        {
          'action_name': 'test_action_prime_prime',
          'variables': {
            'var7': '<!(echo <(var5)<(var6))',
          },
          'inputs' : [
            '<(var2prime)',
          ],
          'outputs': [
            '<(var4prime)',
            '<(var7)',
          ],
          'action': [
            'echo',
            '<(_inputs)',
            '<(_outputs)',
          ],
        },
      ],
    },
  ],
}
