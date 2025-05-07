'use strict';
const common = require('../common');

const repl = require('repl');
const r = repl.start({ terminal: false });
r.setupHistory('/nonexistent/file', common.mustSucceed());
process.stdin.unref?.();
