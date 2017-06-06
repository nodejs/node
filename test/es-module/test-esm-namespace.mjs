// Flags: --experimental-modules
/* eslint-disable required-modules */

import * as fs from 'fs';
import assert from 'assert';

assert.deepStrictEqual(Object.keys(fs), ['default']);
