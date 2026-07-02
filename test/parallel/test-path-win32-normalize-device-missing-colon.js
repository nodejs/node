'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');

if (!common.isWindows)
  common.skip('this test is for win32 only');

// 1. Basic cases for reserved names without colon (Both prefixes)
assert.strictEqual(path.win32.normalize('\\\\.\\CON'), '\\\\.\\CON');
assert.strictEqual(path.win32.normalize('\\\\?\\PRN'), '\\\\?\\PRN');

// 2. Mixed slashes check (Ensuring isPathSeparator works for both prefixes)
assert.strictEqual(
  path.win32.normalize('\\\\.\\CON/file.txt'),
  '\\\\.\\CON\\file.txt'
);
assert.strictEqual(
  path.win32.normalize('\\\\?\\PRN/folder/sub'),
  '\\\\?\\PRN\\folder\\sub'
);

// 3. Alternate Data Streams (Testing prefix symmetry for ADS)
assert.strictEqual(
  path.win32.normalize('\\\\.\\CON\\file:ADS'),
  '\\\\.\\CON\\file:ADS'
);
assert.strictEqual(
  path.win32.normalize('\\\\?\\PRN\\data:stream'),
  '\\\\?\\PRN\\data:stream'
);

// 4. Negative cases (Preventing over-matching)
// These should stay as-is because they are not exact reserved names
assert.strictEqual(path.win32.normalize('\\\\.\\CON-prefix'), '\\\\.\\CON-prefix');
assert.strictEqual(path.win32.normalize('\\\\?\\PRN-suffix'), '\\\\?\\PRN-suffix');

// 5. Join behavior (Ensuring the device acts as a persistent root)
const joined = path.win32.join('\\\\.\\CON', '..');
assert.strictEqual(joined, '\\\\.\\');

// 6. Cover root WITH colon (To cover line 413 in image_9e987d.png)
assert.strictEqual(path.win32.normalize('\\\\?\\CON:'), '\\\\?\\CON:\\');

// 7. Cover path WITHOUT any colon (To cover line 490 in image_9e9859.png)
assert.strictEqual(path.win32.normalize('CON'), 'CON');

// 8. Cover path WITH colon but NOT reserved (To cover partial branch at 490)
assert.strictEqual(path.win32.normalize('C:file.txt'), 'C:file.txt');

// 9. Ensure reserved names with colons get the .\ prefix (To fully green line 490)
assert.strictEqual(path.win32.normalize('CON:file'), '.\\CON:file');
