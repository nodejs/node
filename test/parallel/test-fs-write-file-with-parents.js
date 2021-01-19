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
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

{
  // Test appendFile, appendFileSync, writeFile, writeFileSync
  // with lazy-mkdirp.
  [
    'appendFile',
    'writeFile'
  ].forEach((functionName) => {
    const functionNameSync = functionName + 'Sync';
    // Test async lazy-mkdirp multiple times to ensure EEXIST is ignored.
    [
      path.join(tmpdir.path, functionName + '/mkdirp/test1.txt'),
      path.join(tmpdir.path, functionName + '/mkdirp/test2.txt'),
      path.join(tmpdir.path, functionName + '/mkdirp/test3.txt'),
      path.join(tmpdir.path, functionName + '/mkdirp/test4.txt')
    ].forEach((pathname) => {
      fs[functionName](
        pathname,
        'hello world!',
        { parents: true },
        common.mustSucceed(() => {
          fs.readFile(pathname, 'utf8', common.mustSucceed((data) => {
            // Validate data.
            assert.strictEqual(data, 'hello world!');
          }));
        })
      );
    });
    // Test sync lazy-mkdirp multiple times to ensure EEXIST is ignored.
    [
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test1.txt'),
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test2.txt'),
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test3.txt'),
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test4.txt')
    ].forEach((pathname) => {
      fs[functionNameSync](
        pathname,
        'hello world!',
        { parents: true }
      );
      fs.readFile(pathname, 'utf8', common.mustSucceed((data) => {
        // Validate data.
        assert.strictEqual(data, 'hello world!');
      }));
    });
  });
}

{
  // Test appendFile, appendFileSync, writeFile, writeFileSync
  // will disables option mkdirp for flags missing O_CREAT.
  [
    'appendFile',
    'writeFile'
  ].forEach((functionName) => {
    const functionNameSync = functionName + 'Sync';
    [
      'r',   // O_RDONLY
      'rs',  // Fall through.
      'sr',  // O_RDONLY | O_SYNC
      'r+',  // O_RDWR
      'rs+', // Fall through.
      'sr+'  // O_RDWR | O_SYNC
    ].forEach((flag) => {
      // Test fs.writeFile(..., { flag: 'r', parents: 'true' }, callback)
      // fails with 'ENOENT'.
      fs[functionName](
        path.join(tmpdir.path, 'enoent/enoent/test.txt'),
        'hello world!',
        { flag, parents: true },
        common.mustCall((err) => {
          // Validate error 'ENOENT'.
          assert.strictEqual(err.code, 'ENOENT');
        })
      );
      // Test fs.writeFileSync(..., { flag: 'r', parents: 'true' })
      // fails with 'ENOENT'.
      try {
        fs[functionNameSync](
          path.join(tmpdir.path, 'enoent/enoent/test.txt'),
          'hello world!',
          { flag, parents: true }
        );
        throw new Error('fs.' + functionNameSync + ' should have failed.');
      } catch (errCaught) {
        // Validate error 'ENOENT'.
        assert.strictEqual(errCaught.code, 'ENOENT');
      }
    });
  });
}

{
  // Test open, openSync
  // with lazy-mkdirp.
  [
    'open'
  ].forEach((functionName) => {
    const functionNameSync = functionName + 'Sync';
    // Test async lazy-mkdirp multiple times to ensure EEXIST is ignored.
    [
      path.join(tmpdir.path, functionName + '/mkdirp/test1.txt'),
      path.join(tmpdir.path, functionName + '/mkdirp/test2.txt'),
      path.join(tmpdir.path, functionName + '/mkdirp/test3.txt'),
      path.join(tmpdir.path, functionName + '/mkdirp/test4.txt')
    ].forEach((pathname) => {
      fs[functionName](pathname, {
        flag: 'w',
        parents: true
      }, common.mustSucceed((fd) => {
        // cleanup fd
        fs.close(fd, common.mustSucceed(() => {}));
      }));
    });
    // Test sync lazy-mkdirp multiple times to ensure EEXIST is ignored.
    [
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test1.txt'),
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test2.txt'),
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test3.txt'),
      path.join(tmpdir.path, functionNameSync + '/mkdirp/test4.txt')
    ].forEach((pathname) => {
      const fd = fs[functionNameSync](pathname, {
        flag: 'w',
        parents: true
      });
      // cleanup fd
      fs.close(fd, common.mustSucceed(() => {}));
    });
    // Test promisified lazy-mkdirp multiple times to ensure EEXIST is ignored.
    [
      path.join(tmpdir.path, functionName + 'Promisified/mkdirp/test1.txt'),
      path.join(tmpdir.path, functionName + 'Promisified/mkdirp/test2.txt'),
      path.join(tmpdir.path, functionName + 'Promisified/mkdirp/test3.txt'),
      path.join(tmpdir.path, functionName + 'Promisified/mkdirp/test4.txt')
    ].forEach(async (pathname) => {
      const fd = await fs.promises[functionName](pathname, {
        flag: 'w',
        parents: true
      });
      // cleanup fd
      await fd.close();
    });
  });
}

{
  // Test open, openSync, fs.promises.open
  // will disable option mkdirp for flags missing O_CREAT.
  [
    'open'
  ].forEach((functionName) => {
    const functionNameSync = functionName + 'Sync';
    [
      'r',   // O_RDONLY
      'rs',  // Fall through.
      'sr',  // O_RDONLY | O_SYNC
      'r+',  // O_RDWR
      'rs+', // Fall through.
      'sr+'  // O_RDWR | O_SYNC
    ].forEach(async (flag) => {
      // Test fs.open(..., { flag: 'r', parents: 'true' }, callback)
      // fails with 'ENOENT'.
      fs[functionName](
        path.join(tmpdir.path, 'enoent/enoent/test.txt'),
        { flag, parents: 'true' },
        common.mustCall((err) => {
          // Validate error 'ENOENT'.
          assert.strictEqual(err.code, 'ENOENT');
        })
      );
      // Test fs.openSync(..., { flag: 'r', parents: 'true' })
      // fails with 'ENOENT'.
      try {
        fs[functionNameSync](
          path.join(tmpdir.path, 'enoent/enoent/test.txt'),
          { flag, parents: 'true' },
        );
        throw new Error('fs.' + functionNameSync + ' should have failed.');
      } catch (errCaught) {
        // Validate error 'ENOENT'.
        assert.strictEqual(errCaught.code, 'ENOENT');
      }
      // Test fs.promises.open(..., { flag: 'r', parents: 'true' })
      // fails with 'ENOENT'.
      try {
        await fs.promises[functionName](
          path.join(tmpdir.path, 'enoent/enoent/test.txt'),
          { flag, parents: 'true' },
        );
        throw new Error('fs.' + functionName + ' should have failed.');
      } catch (errCaught) {
        // Validate error 'ENOENT'.
        assert.strictEqual(errCaught.code, 'ENOENT');
      }
    });
  });
}
