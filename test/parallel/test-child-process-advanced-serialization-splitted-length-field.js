'use strict';
const common = require('../common');
const child_process = require('child_process');

// Regression test for https://github.com/nodejs/node/issues/55834
const msgLen = 65521;
let cnt = 10;

if (process.argv[2] === 'child') {
  const msg = Buffer.allocUnsafe(msgLen);
  (function send() {
    if (cnt--) {
      process.send(msg, send);
    } else {
      process.nextTick(() => {
        process.exit(0);
      });  
    }
  })()  
} else {
  const child = child_process.spawn(process.execPath, [__filename, 'child'], {
    stdio: ['inherit', 'inherit', 'inherit', 'ipc'],
    serialization: 'advanced'
  });
  child.on('message', common.mustCall(cnt));
}
