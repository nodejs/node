// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */

import '../common/index';
import * as fs from 'fs';
import assert from 'assert';

assert.deepStrictEqual(Object.keys(fs), ['default']);
