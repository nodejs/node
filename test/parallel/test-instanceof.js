'use strict';
require('../common');
const assert = require('assert');

const F = () => {};
F.prototype = {};
assert(Object.create(F.prototype) instanceof F);
