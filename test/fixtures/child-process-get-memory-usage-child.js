'use strict';

const keepAlive = setInterval(() => {}, 1000);

process.on('message', (msg) => {
  if (msg === 'exit') {
    clearInterval(keepAlive);
    process.exit(0);
  }
});

if (typeof process.send === 'function') {
  process.send('ready');
}
