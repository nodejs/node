
var execFile = require('child_process').execFile,
    path = require('path'),
    childPath = path.join(__dirname, 'child-process-persistent.js');

var child = execFile(process.execPath, [ childPath ], {
  detached: true,
  stdio: 'ignore'
});

console.log(child.pid);

child.unref();
