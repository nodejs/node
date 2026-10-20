'use strict';
require('../common');
const { startNewREPLServer } = require('../common/repl');

// This test ensures that the repl does not
// crash or emit error when throwing `null|undefined`
// ie `throw null` or `throw undefined`.

const { replServer } = startNewREPLServer();

// Should not throw.
replServer.write('throw null\n');
replServer.write('throw undefined\n');
replServer.write('.exit\n');
