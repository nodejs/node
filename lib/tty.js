var spawn = require('child_process').spawn;
var binding = process.binding('stdio');


exports.isatty = binding.isatty;
exports.setRawMode = binding.setRawMode;
exports.getWindowSize = binding.getWindowSize;
exports.setWindowSize = binding.setWindowSize;


exports.open = function(path, args) {
  var fds = binding.openpty();

  var slaveFD = fds[0];
  var masterFD = fds[1];

  var env = { TERM: 'vt100' };
  for (var k in process.env) {
    env[k] = process.env[k];
  }

  var stream = require('net').Stream(slaveFD);
  stream.readable = stream.writable = true;
  stream.resume();


  child = spawn(path, args, {
    env: env,
    customFds: [masterFD, masterFD, masterFD],
    setuid: true
  });

  return [stream, child];
};




