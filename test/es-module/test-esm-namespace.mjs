// Flags: --experimental-modules
import '../common';
import * as fs from 'fs';
import assert from 'assert';

assert.deepStrictEqual(Object.keys(fs), ['default']);
