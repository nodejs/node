// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-harmony-regexp-exec

class MyError extends Error { }
RegExp.prototype.exec = () => { throw new MyError() };
assertEquals(null, "foo".match(/bar/));
