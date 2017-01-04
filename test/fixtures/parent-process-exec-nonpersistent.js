'use strict';
const execFile = require('child_process').execFile;
const path = require('path');
const childPath = path.join(__dirname, 'child-process-persistent.js');

var child = execFile(process.execPath, [ childPath ], {
  detached: true,
  stdio: 'ignore'
});

process.send(child.pid);

child.unref();
