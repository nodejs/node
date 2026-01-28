'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');

// هذا السطر يمنع تشغيل الاختبار على غير ويندوز لأنه خاص بويندوز فقط
if (!common.isWindows)
  common.skip('this test is for win32 only');

// Test cases for reserved device names missing the trailing colon
// See: https://github.com/nodejs/node/pull/[YOUR_PR_NUMBER] (optional)

// 1. Check \\.\CON (Missing colon)
assert.strictEqual(path.win32.normalize('\\\\.\\CON'), '\\\\.\\CON');
assert.strictEqual(path.win32.normalize('\\\\.\\con'), '\\\\.\\con'); // Case insensitive

// 2. Check \\?\CON (Missing colon)
assert.strictEqual(path.win32.normalize('\\\\?\\CON'), '\\\\?\\CON');
assert.strictEqual(path.win32.normalize('\\\\?\\con'), '\\\\?\\con');

// 3. Check that regular files are NOT affected (Sanity check)
assert.strictEqual(path.win32.normalize('\\\\.\\PhysicalDrive0'), '\\\\.\\PhysicalDrive0');

// 4. Check join behavior (to ensure it acts as a root)
// If it's a root, joining '..' should not strip the device.
const joined = path.win32.join('\\\\.\\CON', '..');
assert.strictEqual(joined, '\\\\.\\CON');
