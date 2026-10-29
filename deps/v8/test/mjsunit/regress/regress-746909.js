// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


eval(`import('modules-skip-2.js');`).then(
    assertUnreachable,
    () => { /* ignore */});
eval(`eval(import('modules-skip-2.js'));`).then(
    assertUnreachable,
    () => { /* ignore */});
