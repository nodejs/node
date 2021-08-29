// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute('load1.js');
loadRelativeToScript('load2.js');
console.log('load.js');

if (!ok)
  throw new Error(`Assertion failed: Some text`);

print("Assertion failed: Some text");
