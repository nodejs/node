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

var assert = require('assert');
var path = require('path');

var winPaths = [
  'C:\\path\\dir\\index.html',
  'C:\\another_path\\DIR\\1\\2\\33\\index',
  'another_path\\DIR with spaces\\1\\2\\33\\index',
  '\\foo\\C:',
  'file',
  '.\\file',

  // unc
  '\\\\server\\share\\file_path',
  '\\\\server two\\shared folder\\file path.zip',
  '\\\\teela\\admin$\\system32',
  '\\\\?\\UNC\\server\\share'
];

var winSpecialCaseFormatTests = [
  [{dir: 'some\\dir'}, 'some\\dir\\'],
  [{base: 'index.html'}, 'index.html'],
  [{}, '']
];

var unixPaths = [
  '/home/user/dir/file.txt',
  '/home/user/a dir/another File.zip',
  '/home/user/a dir//another&File.',
  '/home/user/a$$$dir//another File.zip',
  'user/dir/another File.zip',
  'file',
  '.\\file',
  './file',
  'C:\\foo'
];

var unixSpecialCaseFormatTests = [
  [{dir: 'some/dir'}, 'some/dir/'],
  [{base: 'index.html'}, 'index.html'],
  [{}, '']
];

var errors = [
  {method: 'parse', input: [null], message: /Parameter 'pathString' must be a string, not/},
  {method: 'parse', input: [{}], message: /Parameter 'pathString' must be a string, not object/},
  {method: 'parse', input: [true], message: /Parameter 'pathString' must be a string, not boolean/},
  {method: 'parse', input: [1], message: /Parameter 'pathString' must be a string, not number/},
  {method: 'parse', input: [], message: /Parameter 'pathString' must be a string, not undefined/},
  // {method: 'parse', input: [''], message: /Invalid path/}, // omitted because it's hard to trigger!
  {method: 'format', input: [null], message: /Parameter 'pathObject' must be an object, not/},
  {method: 'format', input: [''], message: /Parameter 'pathObject' must be an object, not string/},
  {method: 'format', input: [true], message: /Parameter 'pathObject' must be an object, not boolean/},
  {method: 'format', input: [1], message: /Parameter 'pathObject' must be an object, not number/},
  {method: 'format', input: [{root: true}], message: /'pathObject.root' must be a string or undefined, not boolean/},
  {method: 'format', input: [{root: 12}], message: /'pathObject.root' must be a string or undefined, not number/},
];

checkParseFormat(path.win32, winPaths);
checkParseFormat(path.posix, unixPaths);
checkErrors(path.win32);
checkErrors(path.posix);
checkFormat(path.win32, winSpecialCaseFormatTests);
checkFormat(path.posix, unixSpecialCaseFormatTests);

function checkErrors(path) {
  errors.forEach(function(errorCase) {
    try {
      path[errorCase.method].apply(path, errorCase.input);
    } catch(err) {
      assert.ok(err instanceof TypeError);
      assert.ok(
        errorCase.message.test(err.message),
        'expected ' + errorCase.message + ' to match ' + err.message
      );
      return;
    }

    assert.fail('should have thrown');
  });
}

function checkParseFormat(path, paths) {
  paths.forEach(function(element, index, array) {
    var output = path.parse(element);
    assert.strictEqual(path.format(output), element);
    assert.strictEqual(output.dir, output.dir ? path.dirname(element) : '');
    assert.strictEqual(output.base, path.basename(element));
    assert.strictEqual(output.ext, path.extname(element));
  });
}

function checkFormat(path, testCases) {
  testCases.forEach(function(testCase) {
    assert.strictEqual(path.format(testCase[0]), testCase[1]);
  });
}
