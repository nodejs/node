'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const options = 'test';
const expectedError = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE_ENCODING',
  type: TypeError,
}, 24);

common.fsTest('readFile', ['path', options, expectedError], { throws: true });

assert.throws(() => {
  fs.readFileSync('path', options);
}, expectedError);

common.fsTest('readdir', ['path', options, expectedError], { throws: true });

assert.throws(() => {
  fs.readdirSync('path', options);
}, expectedError);

common.fsTest('readlink', ['path', options, expectedError], { throws: true });

assert.throws(() => {
  fs.readlinkSync('path', options);
}, expectedError);

common.fsTest(
  'writeFile',
  ['path', 'data', options, expectedError],
  { throws: true }
);

assert.throws(() => {
  fs.writeFileSync('path', 'data', options);
}, expectedError);

common.fsTest(
  'appendFile',
  ['path', 'data', options, expectedError],
  { throws: true }
);

assert.throws(() => {
  fs.appendFileSync('path', 'data', options);
}, expectedError);

assert.throws(() => {
  fs.watch('path', options, common.mustNotCall());
}, expectedError);

common.fsTest('realpath', ['path', options, expectedError], { throws: true });

assert.throws(() => {
  fs.realpathSync('path', options);
}, expectedError);

common.fsTest('mkdtemp', ['path', options, expectedError], { throws: true });

assert.throws(() => {
  fs.mkdtempSync('path', options);
}, expectedError);

assert.throws(() => {
  fs.ReadStream('path', options);
}, expectedError);

assert.throws(() => {
  fs.WriteStream('path', options);
}, expectedError);
