'use strict';

const common = require('../common');
const {
  async: fs
} = require('fs');
const {
  Buffer
} = require('buffer');
const {
  URL
} = require('url');

common.crashOnUnhandledRejection();

common.refreshTmpDir();

fs.readdir(common.fixturesDir)
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.readdir(Buffer.from(common.fixturesDir))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.readdir(new URL(`file://${common.fixturesDir}`))
  .then(common.mustCall())
  .catch(common.mustNotCall());

fs.readdir(`${common.fixturesDir}-NO`)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message:
      `ENOENT: no such file or directory, scandir '${common.fixturesDir}-NO'`
  }));

fs.readdir(Buffer.from(`${common.fixturesDir}-NO`))
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message:
      `ENOENT: no such file or directory, scandir '${common.fixturesDir}-NO'`
  }));

fs.readdir(new URL(`file://${common.fixturesDir}-NO`))
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ENOENT',
    type: Error,
    message:
      `ENOENT: no such file or directory, scandir '${common.fixturesDir}-NO'`
  }));

fs.readdir(`${common.fixturesDir}-\u0000NO`)
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));

fs.readdir(Buffer.from(`${common.fixturesDir}-\u0000NO`))
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type object'
  }));

fs.readdir(new URL(`file://${common.fixturesDir}-\u0000NO`))
  .then(common.mustNotCall())
  .catch(common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
    type: Error,
    message: 'The "path" argument must be of type string without null bytes. ' +
             'Received type string'
  }));
