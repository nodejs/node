// Flags: --expose-internals
'use strict';

require('../common');
const { strictEqual, ok } = require('node:assert');
const { CustomEvent: internalCustomEvent } = require('internal/event_target');

// Global
ok(CustomEvent);

strictEqual(CustomEvent, internalCustomEvent);
