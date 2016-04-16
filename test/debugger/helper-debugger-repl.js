'use strict';
process.env.NODE_DEBUGGER_TIMEOUT = 2000;
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var port = common.PORT + 1337;

var child;
var buffer = '';
var expected = [];
var quit;

function startDebugger(scriptToDebug) {
  scriptToDebug = process.env.NODE_DEBUGGER_TEST_SCRIPT ||
                  common.fixturesDir + '/' + scriptToDebug;

  child = spawn(process.execPath, ['debug', '--port=' + port, scriptToDebug]);

  console.error('./node', 'debug', '--port=' + port, scriptToDebug);

  child.stdout.setEncoding('utf-8');
  child.stdout.on('data', function(data) {
    data = (buffer + data).split('\n');
    buffer = data.pop();
    data.forEach(function(line) {
      child.emit('line', line);
    });
  });
  child.stderr.pipe(process.stderr);

  child.on('line', function(line) {
    line = line.replace(/^(debug> *)+/, '');
    console.log(line);
    assert.ok(expected.length > 0, 'Got unexpected line: ' + line);

    var expectedLine = expected[0].lines.shift();
    assert.ok(line.match(expectedLine) !== null, line + ' != ' + expectedLine);

    if (expected[0].lines.length === 0) {
      var callback = expected[0].callback;
      expected.shift();
      callback && callback();
    }
  });

  var childClosed = false;
  child.on('close', function(code) {
    assert(!code);
    childClosed = true;
  });

  var quitCalled = false;
  quit = function() {
    if (quitCalled || childClosed) return;
    quitCalled = true;
    child.stdin.write('quit');
    child.kill('SIGTERM');
  };

  setTimeout(function() {
    console.error('dying badly buffer=%j', buffer);
    var err = 'Timeout';
    if (expected.length > 0 && expected[0].lines) {
      err = err + '. Expected: ' + expected[0].lines.shift();
    }

    child.on('close', function() {
      console.error('child is closed');
      throw new Error(err);
    });

    quit();
  }, 10000).unref();

  process.once('uncaughtException', function(e) {
    console.error('UncaughtException', e, e.stack);
    quit();
    console.error(e.toString());
    process.exit(1);
  });

  process.on('exit', function(code) {
    console.error('process exit', code);
    quit();
    if (code === 0)
      assert(childClosed);
  });
}

function addTest(input, output) {
  function next() {
    if (expected.length > 0) {
      console.log('debug> ' + expected[0].input);
      child.stdin.write(expected[0].input + '\n');

      if (!expected[0].lines) {
        var callback = expected[0].callback;
        expected.shift();

        callback && callback();
      }
    } else {
      quit();
    }
  }
  expected.push({input: input, lines: output, callback: next});
}

var handshakeLines = [
  /listening on port \d+/,
  /connecting.* ok/
];

var initialBreakLines = [
  /break in .*:1/,
  /1/, /2/, /3/
];

var initialLines = handshakeLines.concat(initialBreakLines);

// Process initial lines
addTest(null, initialLines);

exports.startDebugger = startDebugger;
exports.addTest = addTest;
exports.initialLines = initialLines;
exports.handshakeLines = handshakeLines;
exports.initialBreakLines = initialBreakLines;
