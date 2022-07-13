import '../common/index.mjs';
import assert from 'assert';
import { posix } from 'path';
import pathPosix from 'path/posix';

assert.strictEqual(pathPosix, posix);
