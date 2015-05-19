'use strict';
// This test ensures SourceStream.pipe(DestStream) returns DestStream

var common = require('../common');
var Stream = require('stream').Stream;
var assert = require('assert');
var util = require('util');

var sourceStream = new Stream();
var destStream = new Stream();
var result = sourceStream.pipe(destStream);

assert.strictEqual(result, destStream);

