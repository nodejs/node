// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Requires --expose-externalize-string.
var str = createExternalizableString('external');
try {
  externalizeString(str);
} catch (e) { }


function a() {
  JSON.stringify({firstProp: 12345, secondProp: str}, function replacer() {});
}

function b() {
  var hello = 'hello';
  a();
}

b();
