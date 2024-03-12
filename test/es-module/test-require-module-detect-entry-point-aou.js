// Flags: --experimental-require-module-with-detection --abort-on-uncaught-exception
'use strict';

import { mustCall } from '../common/index.mjs';
const fn = mustCall(() => {
  console.log('hello');
});
fn();
