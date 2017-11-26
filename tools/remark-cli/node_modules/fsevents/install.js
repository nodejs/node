if (process.platform === 'darwin') {
  var spawn = require('child_process').spawn;
  var args = ['install', '--fallback-to-build'];
  var options = {stdio: 'inherit'};
  var child = spawn('node-pre-gyp', args, options);
  child.on('close', process.exit);
}