var util = require('util');

var sysWarning;
if (!sysWarning) {
  sysWarning = 'The "sys" module is now called "util". ' +
               'It should have a similar interface.';
  // Uncomment in 2011
  //util.error(sysWarning);
}

exports.print = util.print;
exports.puts = util.puts;
exports.debug = util.debug;
exports.error = util.error;
exports.inspect = util.inspect;
exports.p = util.p;
exports.log = util.log;
exports.exec = util.exec;
exports.pump = util.pump;
exports.inherits = util.inherits;
