// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testAdvanceLastIndex(initial_last_index_value,
                              expected_final_last_index_value) {
  let exec_call_count = 0;
  let last_index_setter_call_count = 0;
  let final_last_index_value;

  var customRegexp = {
    get flags() {
      return 'gu'
    },
    get lastIndex() {
      return initial_last_index_value;
    },
    set lastIndex(v) {
      last_index_setter_call_count++;
      final_last_index_value = v;
    },
    exec() {
      return (exec_call_count++ == 0) ? [""] : null;
    }
  };

  RegExp.prototype[Symbol.replace].call(customRegexp);

  assertEquals(2, exec_call_count);
  assertEquals(2, last_index_setter_call_count);
  assertEquals(expected_final_last_index_value, final_last_index_value);
}

testAdvanceLastIndex(-1, 1);
testAdvanceLastIndex( 0, 1);
testAdvanceLastIndex(2**31 - 2, 2**31 - 1);
testAdvanceLastIndex(2**31 - 1, 2**31 - 0);
testAdvanceLastIndex(2**32 - 3, 2**32 - 2);
testAdvanceLastIndex(2**32 - 2, 2**32 - 1);
testAdvanceLastIndex(2**32 - 1, 2**32 - 0);
testAdvanceLastIndex(2**53 - 2, 2**53 - 1);
testAdvanceLastIndex(2**53 - 1, 2**53 - 0);
testAdvanceLastIndex(2**53 - 0, 2**53 - 0);
