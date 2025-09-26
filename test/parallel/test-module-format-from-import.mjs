// Similar to ./test-module-format.mjs but imports a CJS file instead.

import '../common/index.mjs';
// FIXME(https://github.com/nodejs/node/issues/59666): this only works
// when there are no customization hooks or the hooks do no override the
// source. Otherwise the re-invented require() kicks in which does not
// properly initialize all the properties of the CJS cache entry.
await import('./test-module-format.js');
