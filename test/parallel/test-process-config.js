'use strict';
require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

// check for existence
assert(process.hasOwnProperty('config'));

// ensure that `process.config` is an Object
assert.strictEqual(Object(process.config), process.config);

const configPath = path.resolve(__dirname, '..', '..', 'config.gypi');
let config = fs.readFileSync(configPath, 'utf8');

// clean up comment at the first line
config = config.split('\n').slice(1).join('\n').replace(/'/g, '"');
config = JSON.parse(config, function(key, value) {
  if (value === 'true') return true;
  if (value === 'false') return false;
  return value;
});

assert.deepStrictEqual(config, process.config);
