'use strict';
require('../common');
const repl = require('repl');
const assert = require('assert');

var replserver = new repl.REPLServer();

// Check when throw null is sent to stream behavior is expected. Stream null is checked before accessing the err
assert.strictEqual( replserver.emit('line', 'throw null'), true );

replserver.emit('line', '.exit');
