import { mustCall } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'assert';
import { execPath } from 'process';

import spawn from './helper.spawnAsPromised.mjs';


spawn(execPath, [
  '--experimental-loader',
  'i-dont-exist',
  fixtures.path('print-error-message.js'),
])
  .then(mustCall(({ code, stderr }) => {
    assert.notStrictEqual(code, 0);

    // Error [ERR_MODULE_NOT_FOUND]: Cannot find package 'i-dont-exist'
    // imported from
    assert.match(stderr, /ERR_MODULE_NOT_FOUND/);
    assert.match(stderr, /'i-dont-exist'/);

    assert.ok(!stderr.includes('Bad command or file name'));
  }));
