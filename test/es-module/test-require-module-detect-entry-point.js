// Flags: --experimental-require-module --experimental-detect-module
'use strict';

import { mustCall } from '../common/index.mjs';
const fn = mustCall(() => {
  console.log('hello');
});
fn();
