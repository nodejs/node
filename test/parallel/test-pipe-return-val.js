'use strict';
// This test ensures SourceStream.pipe(DestStream) returns DestStream

const common = require('../common');
const Stream = require('stream').Stream;
const assert = require('assert');
const util = require('util');

var sourceStream = new Stream();
var destStream = new Stream();
var result = sourceStream.pipe(destStream);

assert.strictEqual(result, destStream);

