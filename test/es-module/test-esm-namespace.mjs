// Flags: --experimental-modules
/* eslint-disable required-modules */

import '../common/index';
import * as fs from 'fs';
import assert from 'assert';

assert.deepStrictEqual(Object.keys(fs), ['default']);
