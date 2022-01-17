'use strict';

require('../common');
const assert = require('assert');

function FakeBuffer() { }
Object.setPrototypeOf(FakeBuffer, Buffer);
Object.setPrototypeOf(FakeBuffer.prototype, Buffer.prototype);

const fb = new FakeBuffer();

assert.throws(function() {
  Buffer.from(fb);
}, TypeError);

assert.throws(function() {
  +Buffer.prototype; // eslint-disable-line no-unused-expressions
}, TypeError);

assert.throws(function() {
  Buffer.compare(fb, Buffer.alloc(0));
}, TypeError);

assert.throws(function() {
  fb.write('foo');
}, TypeError);

assert.throws(function() {
  Buffer.concat([fb, fb]);
}, TypeError);

assert.throws(function() {
  fb.toString();
}, TypeError);

assert.throws(function() {
  fb.equals(Buffer.alloc(0));
}, TypeError);

assert.throws(function() {
  fb.indexOf(5);
}, TypeError);

assert.throws(function() {
  fb.readFloatLE(0);
}, TypeError);

assert.throws(function() {
  fb.writeFloatLE(0);
}, TypeError);

assert.throws(function() {
  fb.fill(0);
}, TypeError);
