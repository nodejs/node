'use strict';
require('../common');
var assert = require('assert');
var fs = require('fs');

[undefined, null, {}, []].forEach((input) => throws(input));

function throws(input) {
  assert.throws(() => fs._toUnixTimestamp(input)
  , 'Error: Cannot parse time: ' + input);
}

[1, '1', Date.now()].forEach((input) => {
  assert.doesNotThrow(() => fs._toUnixTimestamp(input));
});
