// Flags: --js-defer-import-eval

import '../common/index.mjs';
import * as assert from 'assert';

globalThis.eval_list = [];

import defer * as deferred from '../fixtures/es-modules/module-with-module-tree.mjs';

assert.strictEqual(deferred.bar, 64);

delete globalThis.eval_list;
