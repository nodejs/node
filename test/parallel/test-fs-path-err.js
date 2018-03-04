'use strict';

const common = require('../common');
const fs = require('fs');
const URL = require('url').URL;
const util = require('util');

function runPathTests(withSlash, withoutSlash, slashedPath, realName) {

  if (!slashedPath) {
    slashedPath = withSlash;
  } else if (typeof slashedPath === 'boolean') {
    realName = slashedPath;
    slashedPath = withSlash;
  }

  common.expectsError(
    () => {
      fs.readFile(withSlash, common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.readFileSync(withSlash, { flag: 'r' });
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.open(withSlash, 'r', common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.openSync(withSlash, 'r');
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.appendFile(withSlash, 'test data', common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.appendFileSync(withSlash, 'test data');
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.createReadStream(withSlash);
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });


  common.expectsError(
    () => {
      fs.createWriteStream(withSlash);
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.truncate(withSlash, 4, common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.truncateSync(withSlash, 4);
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.writeFile(withSlash, 'test data', common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.writeFileSync(withSlash, 'test data');
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: 'The argument "path" must not end with "/". ' +
               `Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.copyFile(withSlash, withoutSlash, common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: `The argument "${realName ? 'src' : 'path'}" must not end ` +
               `with "/". Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.copyFile(withoutSlash, withSlash, common.mustNotCall());
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: `The argument "${realName ? 'dest' : 'path'}" must not end ` +
               `with "/". Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.copyFileSync(withSlash, withoutSlash);
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: `The argument "${realName ? 'src' : 'path'}" must not end ` +
               `with "/". Received ${util.inspect(slashedPath)}`
    });

  common.expectsError(
    () => {
      fs.copyFileSync(withoutSlash, withSlash);
    },
    {
      code: 'ERR_PATH_IS_DIRECTORY',
      type: Error,
      message: `The argument "${realName ? 'dest' : 'path'}" must not end ` +
               `with "/". Received ${util.inspect(slashedPath)}`
    });
}

const path = '/tmp/test';
const stringWithSlash = common.isWindows ? `file:///c:${path}/` :
  `file://${path}/`;
const stringWithoutSlash = common.isWindows ? `file:///c:${path}` :
  `file://${path}`;
const urlWithSlash = new URL(stringWithSlash);
const urlWithoutSlash = new URL(stringWithoutSlash);
const U8ABufferWithSlash = new Uint8Array(Buffer.from(stringWithSlash));
const U8ABufferWithoutSlash = new Uint8Array(Buffer.from(stringWithoutSlash));
runPathTests(urlWithSlash, urlWithoutSlash, `${path}/`);
runPathTests(stringWithSlash, stringWithoutSlash, true);
runPathTests(U8ABufferWithSlash, U8ABufferWithoutSlash, true);
