// Flags: --expose-internals
'use strict';

// This tests that the internal assert module works as expected.
// The failures are tested in test/message.

require('../common');

const internalAssert = require('internal/assert');

// Should not throw.
internalAssert(true);
internalAssert(true, 'fhqwhgads');
