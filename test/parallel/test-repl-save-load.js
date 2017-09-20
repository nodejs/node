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

'use strict';
const common = require('../common');
const assert = require('assert');
const join = require('path').join;
const fs = require('fs');

common.refreshTmpDir();

const repl = require('repl');

const works = [['inner.one'], 'inner.o'];

const putIn = new common.ArrayStream();
const testMe = repl.start('', putIn);


const testFile = [
  'var top = function() {',
  'var inner = {one:1};'
];
const saveFileName = join(common.tmpDir, 'test.save.js');

// input some data
putIn.run(testFile);

// save it to a file
putIn.run([`.save ${saveFileName}`]);

// the file should have what I wrote
assert.strictEqual(fs.readFileSync(saveFileName, 'utf8'),
                   `${testFile.join('\n')}\n`);

{
  // save .editor mode code
  const cmds = [
    'function testSave() {',
    'return "saved";',
    '}'
  ];
  const putIn = new common.ArrayStream();
  const replServer = repl.start('', putIn);

  putIn.run(['.editor']);
  putIn.run(cmds);
  replServer.write('', { ctrl: true, name: 'd' });

  putIn.run([`.save ${saveFileName}`]);
  replServer.close();
  assert.strictEqual(fs.readFileSync(saveFileName, 'utf8'),
                     `${cmds.join('\n')}\n`);
}

// make sure that the REPL data is "correct"
// so when I load it back I know I'm good
testMe.complete('inner.o', function(error, data) {
  assert.deepStrictEqual(data, works);
});

// clear the REPL
putIn.run(['.clear']);

// Load the file back in
putIn.run([`.load ${saveFileName}`]);

// make sure that the REPL data is "correct"
testMe.complete('inner.o', function(error, data) {
  assert.deepStrictEqual(data, works);
});

// clear the REPL
putIn.run(['.clear']);

let loadFile = join(common.tmpDir, 'file.does.not.exist');

// should not break
putIn.write = function(data) {
  // make sure I get a failed to load message and not some crazy error
  assert.strictEqual(data, `Failed to load:${loadFile}\n`);
  // eat me to avoid work
  putIn.write = () => {};
};
putIn.run([`.load ${loadFile}`]);

// throw error on loading directory
loadFile = common.tmpDir;
putIn.write = function(data) {
  assert.strictEqual(data, `Failed to load:${loadFile} is not a valid file\n`);
  putIn.write = () => {};
};
putIn.run([`.load ${loadFile}`]);

// clear the REPL
putIn.run(['.clear']);

// NUL (\0) is disallowed in filenames in UNIX-like operating systems and
// Windows so we can use that to test failed saves
const invalidFileName = join(common.tmpDir, '\0\0\0\0\0');

// should not break
putIn.write = function(data) {
  // make sure I get a failed to save message and not some other error
  assert.strictEqual(data, `Failed to save:${invalidFileName}\n`);
  // reset to no-op
  putIn.write = () => {};
};

// save it to a file
putIn.run([`.save ${invalidFileName}`]);
