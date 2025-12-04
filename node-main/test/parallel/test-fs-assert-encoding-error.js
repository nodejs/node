'use strict';
const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const tmpdir = require('../common/tmpdir');

const testPath = tmpdir.resolve('assert-encoding-error');
const options = 'test';
const expectedError = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
};

assert.throws(() => {
  fs.readFile(testPath, options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.readFileSync(testPath, options);
}, expectedError);

assert.throws(() => {
  fs.readdir(testPath, options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.readdirSync(testPath, options);
}, expectedError);

assert.throws(() => {
  fs.readlink(testPath, options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.readlinkSync(testPath, options);
}, expectedError);

assert.throws(() => {
  fs.writeFile(testPath, 'data', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.writeFileSync(testPath, 'data', options);
}, expectedError);

assert.throws(() => {
  fs.appendFile(testPath, 'data', options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.appendFileSync(testPath, 'data', options);
}, expectedError);

assert.throws(() => {
  fs.watch(testPath, options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.realpath(testPath, options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.realpathSync(testPath, options);
}, expectedError);

assert.throws(() => {
  fs.mkdtemp(testPath, options, common.mustNotCall());
}, expectedError);

assert.throws(() => {
  fs.mkdtempSync(testPath, options);
}, expectedError);

assert.throws(() => {
  fs.ReadStream(testPath, options);
}, expectedError);

assert.throws(() => {
  fs.WriteStream(testPath, options);
}, expectedError);
