// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-use-string-forwarding-table --expose-externalize-string
// Flags: --expose-gc

// The main purpose of this test is to make ClusterFuzz aware of the flag and
// provide some interesting input.

const long_key = 'key1234567890abcdefg';
const substr_key = long_key.substring(3,17);
const consstr_key = 'key' + 1234567890 + 'abcdefg';
const integer_index = long_key.substring(3,8);

gc();
gc();

// Test object with in-place properties.
{
  let obj = {
    'key1234567890abcdefg': 'long_key_value',
    '1234567890abcd': 'substr_value',
    12345: 'integer_index'
  };

  assertEquals('long_key_value', obj[long_key]);
  assertEquals('substr_value', obj[substr_key]);
  assertEquals('long_key_value', obj[consstr_key]);
  assertEquals('integer_index', obj[integer_index]);
}

// Test object with dictionary properties.
{
  let obj = [];
  for (let i = 0; i < 100; ++i) {
    obj[i] = i;
    obj['XXX' + i] = 'XXX' + i;
  }

  obj['key1234567890abcdefg'] = 'long_key_value';
  obj['1234567890abcd'] = 'substr_value';
  obj[12345] = 'integer_index';

  assertEquals('long_key_value', obj[long_key]);
  assertEquals('substr_value', obj[substr_key]);
  assertEquals('long_key_value', obj[consstr_key]);
  assertEquals('integer_index', obj[integer_index]);

  // Externalize keys.
  // Externalization might fail in various stress modes (the strings might be
  // externalized already).
  try {
    externalizeString(long_key);
    externalizeString(substr_key);
    externalizeString(consstr_key);
    externalizeString(integer_index);
  } catch {}
  assertEquals('long_key_value', obj[long_key]);
  assertEquals('substr_value', obj[substr_key]);
  assertEquals('long_key_value', obj[consstr_key]);
  assertEquals('integer_index', obj[integer_index]);
}
