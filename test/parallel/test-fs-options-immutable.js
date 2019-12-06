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
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

fs.readFile(__filename, options, common.mustCall(errHandler));
fs.readFileSync(__filename, options);

fs.readdir(__dirname, options, common.mustCall(errHandler));
fs.readdirSync(__dirname, options);

if (common.canCreateSymLink()) {
  const sourceFile = path.resolve(tmpdir.path, 'test-readlink');
  const linkFile = path.resolve(tmpdir.path, 'test-readlink-link');

  fs.writeFileSync(sourceFile, '');
  fs.symlinkSync(sourceFile, linkFile);

  fs.readlink(linkFile, options, common.mustCall(errHandler));
  fs.readlinkSync(linkFile, options);
}

{
  const fileName = path.resolve(tmpdir.path, 'writeFile');
  fs.writeFileSync(fileName, 'ABCD', options);
  fs.writeFile(fileName, 'ABCD', options, common.mustCall(errHandler));
}

{
  const fileName = path.resolve(tmpdir.path, 'appendFile');
  fs.appendFileSync(fileName, 'ABCD', options);
  fs.appendFile(fileName, 'ABCD', options, common.mustCall(errHandler));
}

if (!common.isIBMi) { // IBMi does not suppport fs.watch()
  const watch = fs.watch(__filename, options, common.mustNotCall());
  watch.close();
}

{
  fs.watchFile(__filename, options, common.mustNotCall());
  fs.unwatchFile(__filename);
}

{
  fs.realpathSync(__filename, options);
  fs.realpath(__filename, options, common.mustCall(errHandler));
}

{
  const tempFileName = path.resolve(tmpdir.path, 'mkdtemp-');
  fs.mkdtempSync(tempFileName, options);
  fs.mkdtemp(tempFileName, options, common.mustCall(errHandler));
}

{
  const fileName = path.resolve(tmpdir.path, 'streams');
  fs.WriteStream(fileName, options).once('open', common.mustCall(() => {
    fs.ReadStream(fileName, options).destroy();
  })).end();
}
