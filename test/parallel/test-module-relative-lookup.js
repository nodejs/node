'use strict';

require('../common');
const assert = require('assert');
const _module = require('module'); // avoid collision with global.module
const lookupResults = _module._resolveLookupPaths('./lodash');
const paths = lookupResults[1];

assert.strictEqual(paths[0], '.',
                   'Current directory gets highest priority for local modules');
