import '../common';
import assert from 'assert';

import { a } from 'https://raw.githubusercontent.com/kuroljov/esm-test/master/a.js';
import { b } from 'https://raw.githubusercontent.com/kuroljov/esm-test/master/b.js';

assert.strictEqual(a, 'a');
assert.strictEqual(b(), 'b');
