var spawn = require('child_process').spawn;
var common = require('../common');
var assert = require('assert');
var path = require('path');
var os = require('os');
var fs = require('fs');

var filename = path.join(common.tmpDir, 'node-history.json');

var config = {
  env: {
    NODE_REPL_HISTORY_FILE: filename,
    NODE_FORCE_READLINE: 1
  }
}

var lhs = spawn(process.execPath, ['-i'], config);
var rhs = spawn(process.execPath, ['-i'], config);

lhs.stdout.pipe(process.stdout);
rhs.stdout.pipe(process.stdout);
lhs.stderr.pipe(process.stderr);
rhs.stderr.pipe(process.stderr);


var i = 0;
var tried = 0;
var exitcount = 0;
while (i++ < 100) {
  lhs.stdin.write('hello = 0' + os.EOL);
  rhs.stdin.write('OK = 0' + os.EOL);
}
lhs.stdin.write('\u0004')
rhs.stdin.write('\u0004')

lhs.once('exit', onexit)
rhs.once('exit', onexit)

function onexit() {
  if (++exitcount !== 2) {
    return;
  }
  check();
}

function check() {
  fs.readFile(filename, 'utf8', function(err, data) {
    if (err) {
      console.log(err)
      if (++tried > 100) {
        throw err;
      }
      return setTimeout(check, 15);
    }
    assert.doesNotThrow(function() {
      console.log(data)
      JSON.parse(data);
    })
  })
}
