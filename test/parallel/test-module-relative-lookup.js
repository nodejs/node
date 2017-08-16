'use strict';

require('../common');
const assert = require('assert');
const _module = require('module'); // avoid collision with global.module
const lookupResults = _module._resolveLookupPaths('./lodash');
let paths = lookupResults[1];

assert.strictEqual(paths[0], '.',
                   'Current directory gets highest priority for local modules');

paths = _module._resolveLookupPaths('./lodash', null, true);

assert.strictEqual(paths && paths[0], '.',
                   'Current directory gets highest priority for local modules');
