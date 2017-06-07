'use strict';
// Refs: https://github.com/nodejs/node/issues/831
//
// Should fail in: 5.8.0
// With output:
/*
Error: This socket is closed.
    at WriteStream.Socket._writeGeneric (net.js:646:19)
    at WriteStream.Socket._write (net.js:698:8)
    at doWrite (_stream_writable.js:301:12)
    at writeOrBuffer (_stream_writable.js:287:5)
    at WriteStream.Writable.write (_stream_writable.js:215:11)
    at WriteStream.Socket.write (net.js:624:40)
    at Console.log (console.js:36:16)
    at ping [as _repeat] (test/known_issues/test-console-unsafe.js:15:11)
    at wrapper [as _onTimeout] (timers.js:275:11)
    at Timer.listOnTimeout (timers.js:92:15)
*/
// Fixed in: ...
require('../common');

delete process.stdout._handle;

console.log('ping');
