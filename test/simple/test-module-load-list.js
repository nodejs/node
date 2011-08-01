// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

// This test makes assertions about exactly which modules and bindings are
// loaded at startup time. We explicitly do NOT load ../common.js or at the
// beginning of this file.

function assertEqual(x, y) {
  if (x !== y) throw new Error("Expected '" + x + "' got '" + y + "'");
}

function checkExpected() {
  assertEqual(expected.length, process.moduleLoadList.length);

  for (var i = 0; i < expected.length; i++) {
    assertEqual(expected[i], process.moduleLoadList[i]);
  }
}

var expected = [
  'Binding evals',
  'Binding natives',
  'NativeModule events',
  'NativeModule buffer',
  'Binding buffer',
  'NativeModule assert',
  'NativeModule util',
  'Binding stdio',
  'NativeModule path',
  'NativeModule module',
  'NativeModule fs',
  'Binding fs',
  'Binding constants',
  'NativeModule stream'
];

checkExpected();


// Now do the test again after we console.log something.
console.log("load console.log");
console.error("load console.error");

if (!process.features.uv)  {
  // legacy
  expected = expected.concat([
    'NativeModule console',
    'NativeModule net_legacy',
    'NativeModule timers_legacy',
    'Binding timer',
    'NativeModule _linklist',
    'Binding net',
    'NativeModule freelist',
    'Binding io_watcher',
    'NativeModule tty',
    'NativeModule tty_posix',
    'NativeModule readline'
  ]);
} else {
  if (process.platform == 'win32') {
    // win32
    expected = expected.concat([
      'NativeModule console',
      'NativeModule tty',
      'NativeModule tty_win32',
      'NativeModule readline'
    ]);
  } else {
    // unix libuv backend.
    expected = expected.concat([
      'NativeModule console',
      'NativeModule net_legacy',
      'NativeModule timers_uv',
      'Binding timer_wrap',
      'NativeModule _linklist',
      'Binding net',
      'NativeModule freelist',
      'Binding io_watcher',
      'NativeModule tty',
      'NativeModule tty_posix',
      'NativeModule readline'
    ]);
  }
}

console.error(process.moduleLoadList)

checkExpected();

