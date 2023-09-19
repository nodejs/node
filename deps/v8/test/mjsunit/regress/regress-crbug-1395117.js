// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --shared-string-table

let o = {}
let json_str = '[{"a": 2.1, "b": 1, "c": "hello"}, {"a": null, "b": 2, "c": {"a": 2.1, "b": 1.1, "c": "hello"}}]';
// Internalize a bunch of strings to trigger a shared GC.
for (let i=0; i<100; i++) {
  let str = 'X'.repeat(100) + i;
  o[str] = str;
}
JSON.parse('[{"a": 2.1, "b": 1, "c": "hello"}, {"a": null, "b": 2, "c": {"a": 2.1, "b": 1.1, "c": "hello"}}]');
