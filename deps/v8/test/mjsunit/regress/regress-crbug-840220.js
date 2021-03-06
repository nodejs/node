// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var boundFunction = (function(){}).bind();

var instance = new Uint8Array()
instance.constructor = {
  [Symbol.species]: boundFunction
};

assertThrows(() => instance.map(each => each * 2), TypeError);
