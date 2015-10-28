'use strict';
// Flags: --expose_internals
const common = require('../common');
const util = require('util');
const JSStream = require('binding/js_stream').JSStream;

// Testing if will abort when properties are printed.
util.inspect(new JSStream());
