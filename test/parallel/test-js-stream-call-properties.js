'use strict';

require('../common');
const util = require('util');
const JSStream = process.binding('js_stream').JSStream;

// Testing if will abort when properties are printed.
util.inspect(new JSStream());
