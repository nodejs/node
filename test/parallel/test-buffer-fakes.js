'use strict';

const common = require('../common');
const assert = require('assert');
const Buffer = require('buffer').Buffer;
const Bp = Buffer.prototype;

function FakeBuffer() { }
FakeBuffer.__proto__ = Buffer;
FakeBuffer.prototype.__proto__ = Buffer.prototype;

const fb = new FakeBuffer();

assert.throws(function() {
  new Buffer(fb);
}, TypeError);

assert.throws(function() {
  +Buffer.prototype;
}, TypeError);

assert.throws(function() {
  Buffer.compare(fb, new Buffer(0));
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
  fb.equals(new Buffer(0));
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
