// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class MyError extends Error { }
RegExp.prototype.exec = () => { throw new MyError() };
assertThrows(() => "foo".match(/bar/), MyError);
