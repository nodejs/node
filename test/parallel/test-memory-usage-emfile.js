'use strict';
require('../common');
var assert = require('assert');

var fs = require('fs');

var files = [];

while (files.length < 256)
  files.push(fs.openSync(__filename, 'r'));

var r = process.memoryUsage();
assert.equal(true, r['rss'] > 0);
