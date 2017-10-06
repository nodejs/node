'use strict';
const common = require('../common');

/*
 * These tests make sure that the `options` object passed to these functions are
 * never altered.
 *
 * Refer: https://github.com/nodejs/node/issues/7655
 */

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const errHandler = (e) => assert.ifError(e);
const options = Object.freeze({});
common.refreshTmpDir();

{
  assert.doesNotThrow(() =>
    fs.readFile(__filename, options, common.mustCall(errHandler))
  );
  assert.doesNotThrow(() => fs.readFileSync(__filename, options));
}

{
  assert.doesNotThrow(() =>
    fs.readdir(__dirname, options, common.mustCall(errHandler))
  );
  assert.doesNotThrow(() => fs.readdirSync(__dirname, options));
}

if (common.canCreateSymLink()) {
  const sourceFile = path.resolve(common.tmpDir, 'test-readlink');
  const linkFile = path.resolve(common.tmpDir, 'test-readlink-link');

  fs.writeFileSync(sourceFile, '');
  fs.symlinkSync(sourceFile, linkFile);

  assert.doesNotThrow(() =>
    fs.readlink(linkFile, options, common.mustCall(errHandler))
  );
  assert.doesNotThrow(() => fs.readlinkSync(linkFile, options));
}

{
  const fileName = path.resolve(common.tmpDir, 'writeFile');
  assert.doesNotThrow(() => fs.writeFileSync(fileName, 'ABCD', options));
  assert.doesNotThrow(() =>
    fs.writeFile(fileName, 'ABCD', options, common.mustCall(errHandler))
  );
}

{
  const fileName = path.resolve(common.tmpDir, 'appendFile');
  assert.doesNotThrow(() => fs.appendFileSync(fileName, 'ABCD', options));
  assert.doesNotThrow(() =>
    fs.appendFile(fileName, 'ABCD', options, common.mustCall(errHandler))
  );
}

{
  let watch;
  assert.doesNotThrow(() => {
    watch = fs.watch(__filename, options, common.mustNotCall());
  });
  watch.close();
}

{
  assert.doesNotThrow(
    () => fs.watchFile(__filename, options, common.mustNotCall())
  );
  fs.unwatchFile(__filename);
}

{
  assert.doesNotThrow(() => fs.realpathSync(__filename, options));
  assert.doesNotThrow(() =>
    fs.realpath(__filename, options, common.mustCall(errHandler))
  );
}

{
  const tempFileName = path.resolve(common.tmpDir, 'mkdtemp-');
  assert.doesNotThrow(() => fs.mkdtempSync(tempFileName, options));
  assert.doesNotThrow(() =>
    fs.mkdtemp(tempFileName, options, common.mustCall(errHandler))
  );
}

{
  const fileName = path.resolve(common.tmpDir, 'streams');
  assert.doesNotThrow(() => {
    fs.WriteStream(fileName, options).once('open', common.mustCall(() => {
      assert.doesNotThrow(() => fs.ReadStream(fileName, options));
    }));
  });
}
