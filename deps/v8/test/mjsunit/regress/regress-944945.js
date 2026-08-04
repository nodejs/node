// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const E = '"use asm";\nfunction f() { LOCALS }\nreturn f;';
const PI = new Function(E.replace('LOCALS', Array(999995).fill('0.9')));
