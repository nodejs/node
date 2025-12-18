// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-externalize-string --expose-gc --allow-natives-syntax

function foo(s) {
  return s.charCodeAt(12);
}

var extern = "internalized dummy";
extern = createExternalizableString(
    '1234567890qiaipppiúöäöáœba' +
    'jalsdjasldjasdlasjdalsdjasldk');
externalizeString(extern);
%PrepareFunctionForOptimization(foo);
assertEquals(97, foo(extern));
assertEquals(97, foo(extern));
%OptimizeFunctionOnNextCall(foo);
assertEquals(97, foo(extern));
