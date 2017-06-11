'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const options = 'test';
const unknownEncodingMessage = /^Error: Unknown encoding: test$/;

assert.throws(() => {
  fs.readFile('path', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.readFileSync('path', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.readdir('path', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.readdirSync('path', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.readlink('path', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.readlinkSync('path', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.writeFile('path', 'data', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.writeFileSync('path', 'data', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.appendFile('path', 'data', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.appendFileSync('path', 'data', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.watch('path', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.realpath('path', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.realpathSync('path', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.mkdtemp('path', options, common.mustNotCall());
}, unknownEncodingMessage);

assert.throws(() => {
  fs.mkdtempSync('path', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.ReadStream('path', options);
}, unknownEncodingMessage);

assert.throws(() => {
  fs.WriteStream('path', options);
}, unknownEncodingMessage);
