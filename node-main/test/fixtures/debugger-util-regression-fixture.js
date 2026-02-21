'use strict';
const util = require('util');
const payload = util.inspect({a: 'b'});
console.log(payload);
