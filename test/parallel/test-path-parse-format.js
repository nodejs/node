'use strict';
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

var errors = [
  {method: 'parse', input: [null],
   message: /Path must be a string. Received null/},
  {method: 'parse', input: [{}],
   message: /Path must be a string. Received {}/},
  {method: 'parse', input: [true],
   message: /Path must be a string. Received true/},
  {method: 'parse', input: [1],
   message: /Path must be a string. Received 1/},
  {method: 'parse', input: [],
   message: /Path must be a string. Received undefined/},
  // {method: 'parse', input: [''],
  //  message: /Invalid path/}, // omitted because it's hard to trigger!
  {method: 'format', input: [null],
   message: /Parameter 'pathObject' must be an object, not/},
  {method: 'format', input: [''],
   message: /Parameter 'pathObject' must be an object, not string/},
  {method: 'format', input: [true],
   message: /Parameter 'pathObject' must be an object, not boolean/},
  {method: 'format', input: [1],
   message: /Parameter 'pathObject' must be an object, not number/},
  {method: 'format', input: [{root: true}],
   message: /'pathObject.root' must be a string or undefined, not boolean/},
  {method: 'format', input: [{root: 12}],
   message: /'pathObject.root' must be a string or undefined, not number/},
];

check(path.win32, winPaths);
check(path.posix, unixPaths);
checkErrors(path.win32);
checkErrors(path.posix);

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


function check(path, paths) {
  paths.forEach(function(element, index, array) {
    var output = path.parse(element);
    assert.strictEqual(path.format(output), element);
    assert.strictEqual(output.dir, output.dir ? path.dirname(element) : '');
    assert.strictEqual(output.base, path.basename(element));
    assert.strictEqual(output.ext, path.extname(element));
  });
}
