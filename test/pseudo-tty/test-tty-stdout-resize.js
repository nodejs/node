'use strict';
const { mustCall } = require('../common');
const { notStrictEqual } = require('assert');

// tty.WriteStream#_refreshSize() only emits the 'resize' event when the
// window dimensions change.  We cannot influence that from the script
// but we can set the old values to something exceedingly unlikely.
process.stdout.columns = 9001;
process.stdout.on('resize', mustCall());
process.kill(process.pid, 'SIGWINCH');
setImmediate(mustCall(() => notStrictEqual(process.stdout.columns, 9001)));
