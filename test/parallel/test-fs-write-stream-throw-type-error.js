'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.tmpDir, '/dummy');
assert.throws(function() { 
  fs.createWriteStream(example, 123); 
}, TypeError, 'options must be a string or an object');
assert.throws(function() { 
  fs.createWriteStream(example, 0); 
}, TypeError, 'options must be a string or an object');
assert.throws(function() { 
  fs.createWriteStream(example, true); 
}, TypeError, 'options must be a string or an object');
assert.throws(function() { 
  fs.createWriteStream(example, false); 
}, TypeError, 'options must be a string or an object');
