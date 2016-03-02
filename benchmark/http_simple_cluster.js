'use strict';
const cluster = require('cluster');
const os = require('os');
const path = require('path');

if (cluster.isMaster) {
  console.log('master running on pid %d', process.pid);
  for (var i = 0, n = os.cpus().length; i < n; ++i) cluster.fork();
} else {
  require(path.join(__dirname, 'http_simple.js'));
}
