// Flags: --expose-internals
import '../common/index.mjs';
import assert from 'assert';
import { lookupService } from 'dns/promises';

const invalidAddress = 'fasdfdsaf';

assert.throws(() => {
  lookupService(invalidAddress, 0);
}, {
  code: 'ERR_INVALID_OPT_VALUE',
  name: 'TypeError',
  message: `The value "${invalidAddress}" is invalid for option "address"`
});
