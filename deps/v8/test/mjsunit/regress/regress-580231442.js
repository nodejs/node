// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string

// 1. Create 2-byte pattern with 1-byte named capture group name
let group_name = ['my_group_', 98765].join('');
let re_pattern = [('\u1234' + '(?<' + group_name + '>a+)').slice(1)].join('');

let re = new RegExp(re_pattern);
let match = re.exec('aaa');

// 2. Create k2 (1-byte) and cons (1-byte ConsString)
let k2 = ['my_group_', 98765].join('');
let cons = k2 + 'c'.repeat(20);

// 3. Internalize k2
let dummy = {};
dummy[k2] = 1;

// 4. Trigger crash
createExternalizableString(cons);
