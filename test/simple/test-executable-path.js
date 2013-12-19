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

var common = require('../common');
var assert = require('assert');
var path = require('path');
var match = false;

var isDebug = process.features.debug;
var isWindows = process.platform === 'win32';

var debugPaths = [path.normalize(path.join(__dirname, '..', '..',
                                           'out', 'Debug', 'node')),
                  path.normalize(path.join(__dirname, '..', '..',
                                           'Debug', 'node'))];
var defaultPaths = [path.normalize(path.join(__dirname, '..', '..',
                                             'out', 'Release', 'node')),
                    path.normalize(path.join(__dirname, '..', '..',
                                             'Release', 'node'))];

console.error('debugPaths: ' + debugPaths);
console.error('defaultPaths: ' + defaultPaths);
console.error('process.execPath: ' + process.execPath);

function pathStartsWith(a, b) {
  if (isWindows)
    return (a.toLowerCase().indexOf(b.toLowerCase()) == 0);
  else
    return (a.indexOf(b) == 0);
}


if (isDebug) {
  debugPaths.forEach(function(path) {
    match = match || pathStartsWith(process.execPath, path);
  });
} else {
  defaultPaths.forEach(function(path) {
    match = match || pathStartsWith(process.execPath, path);
  });
}

assert.ok(match);
