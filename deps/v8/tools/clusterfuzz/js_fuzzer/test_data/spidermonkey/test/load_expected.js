// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: resources/spidermonkey_stubs.js
var document = {
  all: undefined
};

// Original: spidermonkey/shell.js
console.log('/shell.js');
if (!ok) ;

// Original: spidermonkey/test/shell.js
console.log('/test/shell.js');
function testFun(test) {
  if (test) ;
}
testFun(() => {
  ;
});

// Original: spidermonkey/load1.js
console.log('load1.js');

// Original: spidermonkey/test/load2.js
console.log('load2.js');

// Original: spidermonkey/test/load.js
console.log('load.js');
if (!ok) throw new Error(`*****tion failed: Some text`);
print("*****tion failed: Some text");
try {
  if (test) {
    throw Exception();
  }
} catch (e) {}
check()`\01`;
