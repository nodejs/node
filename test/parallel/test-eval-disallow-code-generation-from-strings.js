// Flags: --disallow-code-generation-from-strings
'use strict';

require('../common');
const assert = require('assert');

// Verify that v8 option --disallow-code-generation-from-strings is still
// respected
assert.throws(() => eval('"eval"'), EvalError);
