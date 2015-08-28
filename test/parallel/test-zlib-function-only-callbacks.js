'use strict';
var common = require('../common');
var assert = require('assert');
var zlib = require('zlib');

// test callback in `onEnd`
zlib.deflate('abc', {}, 'not a function');
// test callback in `onError`
zlib.deflate({}, {}, 'not a function');
