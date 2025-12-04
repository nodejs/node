// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f1() {}
function f2() {}
async function f3() {}
async function f4() {}

// Here we want mostly non-async replacements in practice. E.g. in this case
// there's just f2.
f1();
f1();
f1();
f1();
f1();
f1();
f1();

// Here we want mostly async replacements in practice. E.g. in this case
// there's just f4.
f3();
f3();
f3();
f3();
f3();
f3();
f3();
