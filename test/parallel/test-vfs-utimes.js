// Flags: --experimental-vfs
'use strict';

// utimes / lutimes accept Date instances, numeric seconds, strings,
// and other values (which fall through to a no-op time value).

require('../common');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/u.txt', 'x');
myVfs.symlinkSync('/u.txt', '/lk');

// Numeric seconds branch
myVfs.utimesSync('/u.txt', 1000, 2000);

// Date object branch
myVfs.utimesSync('/u.txt', new Date(3000000), new Date(4000000));

// String time → DateNow() fallback
myVfs.utimesSync('/u.txt', 'now', 'now');

// null/undefined → fallthrough (returns the value as-is)
myVfs.utimesSync('/u.txt', null, undefined);

// lutimes for symlinks
myVfs.lutimesSync('/lk', new Date(0), new Date(0));
