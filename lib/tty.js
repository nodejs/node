var spawn = require('child_process').spawn;
var binding = process.binding('stdio');


exports.isatty = binding.isatty;
exports.setRawMode = binding.setRawMode;
exports.getColumns = binding.getColumns;


exports.open = function(path, args) {
  var fds = binding.openpty();

  var masterFD = fds[1];
  var slaveFD = fds[0];

  var env = { TERM: 'vt100' };
  for (var k in process.env) {
    env[k] = process.env[k];
  }

  child = spawn(path, args, env, [masterFD, masterFD, masterFD]);

  return [slaveFD, child];
};




