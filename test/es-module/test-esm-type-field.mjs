import '../common/index.mjs';
import cjs from '../fixtures/baz.js';
import { message } from '../fixtures/es-modules/message.mjs';
import assert from 'assert';

// Assert we loaded esm dependency as ".js" in this mode
assert.strictEqual(message, 'A message');
// Assert we loaded CommonJS dependency
assert.strictEqual(cjs, 'perhaps I work');
