'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const options = 'test';
const expectedError = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE_ENCODING',
  type: TypeError,
}, 17);

assert.throws(() => {
  fs.readFile('path', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.readFileSync('path', options);
}, expectedError);

assert.throws(() => {
  fs.readdir('path', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.readdirSync('path', options);
}, expectedError);

assert.throws(() => {
  fs.readlink('path', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.readlinkSync('path', options);
}, expectedError);

assert.throws(() => {
  fs.writeFile('path', 'data', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.writeFileSync('path', 'data', options);
}, expectedError);

assert.throws(() => {
  fs.appendFile('path', 'data', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.appendFileSync('path', 'data', options);
}, expectedError);

assert.throws(() => {
  fs.watch('path', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.realpath('path', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.realpathSync('path', options);
}, expectedError);

assert.throws(() => {
  fs.mkdtemp('path', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.mkdtempSync('path', options);
}, expectedError);

assert.throws(() => {
  fs.ReadStream('path', options);
}, expectedError);

assert.throws(() => {
  fs.WriteStream('path', options);
}, expectedError);
