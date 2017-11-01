// Flags: --experimental-modules
/* eslint-disable required-modules */

import assert from 'assert';
import fs, { readFile } from 'fs';

assert(fs);
assert(fs.readFile);
assert(readFile);
