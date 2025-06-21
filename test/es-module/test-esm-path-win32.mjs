import '../common/index.mjs';
import assert from 'assert';
import { win32 } from 'path';
import pathWin32 from 'path/win32';

assert.strictEqual(pathWin32, win32);
