import { mustCall } from '../common/index.mjs';
import { strictEqual } from 'assert';

import { importFixture } from '../fixtures/pkgexports.mjs';

importFixture('as%2Ff').catch(mustCall((err) => {
  strictEqual(err.code, 'ERR_INVALID_MODULE_SPECIFIER');
}));

importFixture('as%5Cf').catch(mustCall((err) => {
  strictEqual(err.code, 'ERR_INVALID_MODULE_SPECIFIER');
}));

importFixture('as\\df').catch(mustCall((err) => {
  strictEqual(err.code, 'ERR_INVALID_MODULE_SPECIFIER');
}));

importFixture('@as@df').catch(mustCall((err) => {
  strictEqual(err.code, 'ERR_INVALID_MODULE_SPECIFIER');
}));
