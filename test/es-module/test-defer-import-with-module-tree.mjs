// Flags: --js-defer-import-eval

import '../common/index.mjs';

import defer * as deferred from '../fixtures/es-modules/module-with-module-tree.mjs';

console.log(deferred.bar);

delete globalThis.eval_list;
