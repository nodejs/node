// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-unknown-builtin-module.mjs
import { expectsError, mustCall } from '../common/index.mjs';
import assert from 'assert';

const unknownBuiltinModule = 'unknown-builtin-module';

import(unknownBuiltinModule)
.then(assert.fail, expectsError({
  code: 'ERR_UNKNOWN_BUILTIN_MODULE',
  message: `No such built-in module: ${unknownBuiltinModule}`
}))
.then(mustCall());
