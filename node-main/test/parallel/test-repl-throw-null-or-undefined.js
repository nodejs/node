'use strict';
require('../common');

// This test ensures that the repl does not
// crash or emit error when throwing `null|undefined`
// ie `throw null` or `throw undefined`.

const r = require('repl').start();

// Should not throw.
r.write('throw null\n');
r.write('throw undefined\n');
r.write('.exit\n');
