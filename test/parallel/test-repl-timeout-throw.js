'use strict';
const common = require('../common');
var assert = require('assert');

var spawn = require('child_process').spawn;

var child = spawn(process.execPath, [ '-i' ], {
  stdio: [null, null, 2]
});

var stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', function(c) {
  process.stdout.write(c);
  stdout += c;
});

child.stdin.write = function(original) {
  return function(c) {
    process.stderr.write(c);
    return original.call(child.stdin, c);
  };
}(child.stdin.write);

child.stdout.once('data', function() {
  child.stdin.write('var throws = 0;');
  child.stdin.write('process.on("exit",function(){console.log(throws)});');
  child.stdin.write('function thrower(){console.log("THROW",throws++);XXX};');
  child.stdin.write('setTimeout(thrower);""\n');

  setTimeout(fsTest, 50);
  function fsTest() {
    var f = JSON.stringify(__filename);
    child.stdin.write('fs.readFile(' + f + ', thrower);\n');
    setTimeout(eeTest, 50);
  }

  function eeTest() {
    child.stdin.write('setTimeout(function() {\n' +
                      '  var events = require("events");\n' +
                      '  var e = new events.EventEmitter;\n' +
                      '  process.nextTick(function() {\n' +
                      '    e.on("x", thrower);\n' +
                      '    setTimeout(function() {\n' +
                      '      e.emit("x");\n' +
                      '    });\n' +
                      '  });\n' +
                      '});"";\n');

    setTimeout(child.stdin.end.bind(child.stdin), common.platformTimeout(200));
  }
});

child.on('close', function(c) {
  assert(!c);
  // make sure we got 3 throws, in the end.
  var lastLine = stdout.trim().split(/\r?\n/).pop();
  assert.equal(lastLine, '> 3');
});
