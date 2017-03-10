const spawn = require('child_process').spawn;
const path = require('path');
const childPath = path.join(__dirname, 'child-process-persistent.js');

const child = spawn(process.execPath, [ childPath ], {
  detached: true,
  stdio: 'ignore'
});

console.log(child.pid);

child.unref();
