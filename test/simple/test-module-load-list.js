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
  if (x !== y) throw new Error('Expected \'' + x + '\' got \'' + y + '\'');
}

function checkExpected() {
  var toCompare = Math.max(expected.length, process.moduleLoadList.length);
  for (var i = 0; i < toCompare; i++) {
    if (expected[i] !== process.moduleLoadList[i]) {
      console.error('process.moduleLoadList[' + i + '] = ' +
                    process.moduleLoadList[i]);
      console.error('expected[' + i + '] = ' + expected[i]);

      console.error('process.moduleLoadList', process.moduleLoadList);
      console.error('expected = ', expected);
      throw new Error('mismatch');
    }
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
  'NativeModule path',
  'NativeModule module',
  'NativeModule fs',
  'Binding fs',
  'Binding constants',
  'NativeModule stream'
];

checkExpected();


// Now do the test again after we console.log something.
console.log('load console.log. process.stdout._type is ' +
    process.stdout._type);

switch (process.stdout._type) {
  case 'fs':
    expected = expected.concat([
      'NativeModule console',
      'Binding tty_wrap'
    ]);
    break;

  case 'tty':
    expected = expected.concat([
      'NativeModule console',
      'Binding tty_wrap',
      'NativeModule tty',
      'NativeModule net',
      'NativeModule timers',
      'Binding timer_wrap',
      'NativeModule _linklist'
    ]);
    break;

  case 'pipe':
    expected = expected.concat([
      'NativeModule console',
      'Binding tty_wrap',
      'NativeModule net',
      'NativeModule timers',
      'Binding timer_wrap',
      'NativeModule _linklist',
      'Binding pipe_wrap'
    ]);
    break;

  default:
    assert.ok(0, 'prcoess.stdout._type is bad');
}

console.error('process.moduleLoadList', process.moduleLoadList);
console.error('expected', expected);

checkExpected();

