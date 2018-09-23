'use strict';
var common = require('../common');
var script = common.fixturesDir + '/breakpoints_utf8.js';
process.env.NODE_DEBUGGER_TEST_SCRIPT = script;

require('./test-debugger-repl.js');

