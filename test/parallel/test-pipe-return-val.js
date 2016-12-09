'use strict';
// This test ensures SourceStream.pipe(DestStream) returns DestStream

require('../common');
var Stream = require('stream').Stream;
var assert = require('assert');

var sourceStream = new Stream();
var destStream = new Stream();
var result = sourceStream.pipe(destStream);

assert.strictEqual(result, destStream);
