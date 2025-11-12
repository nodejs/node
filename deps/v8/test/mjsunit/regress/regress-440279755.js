// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const json_part1 = '{":\\"foo":1}';
const json_part2 = '{"bar":"foo"}';
const json_input = '[' + json_part1 + ',' + json_part2 + ']';
const parsed = JSON.parse(json_input);
const obj = parsed[0];
let s = JSON.stringify(obj);
assertEquals('{":\\\"foo":1}', s);
