'use strict';
const fs = require('fs');
const path = require('path');
const { parentPort } = require('worker_threads');

// A worker mounts the assets too, at the same mount point, so a worker script
// among them, and the code and data next to it, can be loaded from there.
parentPort.postMessage({
  dirname: __dirname,
  greeting: fs.readFileSync(path.join(__dirname, 'data', 'greeting.txt'), 'utf8'),
  sum: require('./modules/math.js').add(2, 3),
});
