// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --promise-extra

'use strict';

Object.defineProperties(Object.prototype, {
  promise: {set: assertUnreachable},
  reject: {set: assertUnreachable},
  resolve: {set: assertUnreachable},
});

class P extends Promise {}

P.all([Promise.resolve('ok')]);
P.race([Promise.resolve('ok')]);
P.defer();
