'use strict';
const { test } = require('node:test');

test('zeta', () => {});

// Even though every test in this file is filtered out by --test-name-pattern,
// a real load-time failure must still be reported (and must not be suppressed
// by the empty-file handling for issue #64099).
throw new Error('intentional load failure');
