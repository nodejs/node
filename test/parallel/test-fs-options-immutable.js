'use strict';

/*
 * These tests make sure that the `options` object passed to these functions are
 * never altered.
 *
 * Refer: https://github.com/nodejs/node/issues/7655
 */

const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const errHandler = (e) => { return assert.ifError(e); };
const options = Object.freeze({});
common.refreshTmpDir();

{
  assert.doesNotThrow(() => {
    return fs.readFile(__filename, options, common.mustCall(errHandler));
  });
  assert.doesNotThrow(() => { return fs.readFileSync(__filename, options); });
}

{
  assert.doesNotThrow(() => {
    return fs.readdir(__dirname, options, common.mustCall(errHandler));
  });
  assert.doesNotThrow(() => { return fs.readdirSync(__dirname, options); });
}

if (common.canCreateSymLink()) {
  const sourceFile = path.resolve(common.tmpDir, 'test-readlink');
  const linkFile = path.resolve(common.tmpDir, 'test-readlink-link');

  fs.writeFileSync(sourceFile, '');
  fs.symlinkSync(sourceFile, linkFile);

  assert.doesNotThrow(() => {
    return fs.readlink(linkFile, options, common.mustCall(errHandler));
  });
  assert.doesNotThrow(() => { return fs.readlinkSync(linkFile, options); });
}

{
  const fileName = path.resolve(common.tmpDir, 'writeFile');
  assert.doesNotThrow(() => {
    return fs.writeFileSync(fileName, 'ABCD', options);
  });
  assert.doesNotThrow(() => {
    return fs.writeFile(fileName, 'ABCD', options, common.mustCall(errHandler));
  });
}

{
  const fileName = path.resolve(common.tmpDir, 'appendFile');
  assert.doesNotThrow(() => {
    return fs.appendFileSync(fileName, 'ABCD', options);
  });
  assert.doesNotThrow(() => {
    return fs.appendFile(
      fileName, 'ABCD', options, common.mustCall(errHandler)
    );
  });
}

if (!common.isAix) {
  // TODO(thefourtheye) Remove this guard once
  // https://github.com/nodejs/node/issues/5085 is fixed
  {
    let watch;
    assert.doesNotThrow(() => {
      return watch = fs.watch(__filename, options, () => {});
    });
    watch.close();
  }

  {
    assert.doesNotThrow(() => {
      return fs.watchFile(__filename, options, () => {});
    });
    fs.unwatchFile(__filename);
  }
}

{
  assert.doesNotThrow(() => { return fs.realpathSync(__filename, options); });
  assert.doesNotThrow(() => {
    return fs.realpath(__filename, options, common.mustCall(errHandler));
  });
}

{
  const tempFileName = path.resolve(common.tmpDir, 'mkdtemp-');
  assert.doesNotThrow(() => { return fs.mkdtempSync(tempFileName, options); });
  assert.doesNotThrow(() => {
    return fs.mkdtemp(tempFileName, options, common.mustCall(errHandler));
  });
}

{
  const fileName = path.resolve(common.tmpDir, 'streams');
  assert.doesNotThrow(() => {
    fs.WriteStream(fileName, options).once('open', () => {
      assert.doesNotThrow(() => { return fs.ReadStream(fileName, options); });
    });
  });
}
