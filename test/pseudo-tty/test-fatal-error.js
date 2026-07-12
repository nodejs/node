'use strict';
require('../common');

const { inspect } = require('util');

inspect.defaultOptions.colors = true;

const err = new TypeError('foobar');
err.bla = true;
throw err;
