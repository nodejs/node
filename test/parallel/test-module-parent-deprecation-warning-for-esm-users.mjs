import { mustCall } from '../common/index.mjs';
import fixtures from '../common/fixtures.js';

// Accessing module.parent from a child module should raise a deprecation
// warning when imported from ESM.
import(fixtures.path('cjs-module-parent.js'))
    .then(mustCall());
