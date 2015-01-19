var common = require('../common');
var assert = require('assert');
var Stream = require('stream');
var repl = require('repl');
var fs = require('fs');
var path = require('path');

process.env.NODE_HISTORY_PATH = path.join('..', 'fixtures', '.node_history');

// create a dummy stream that does nothing
var stream = new Stream();
stream.write = stream.pause = stream.resume = function(){};
stream.readable = stream.writable = true;

function testTerminalMode() {
  var r1 = repl.start({
    terminal: true,
    useHistory: true
  });

  process.stdin.write('blahblah');

  process.nextTick(function() {
    process.stdin.end();
  });

  r1.on('exit', function() {
    console.log('r1 exit')
    var contents = fs.readFileSync(process.env.NODE_HISTORY_PATH, 'utf8');
    fs.unlinkSync(process.env.NODE_HISTORY_PATH);
    assert.equal(contents, 'blahblah');
    testRegularMode();
  });
}

function testRegularMode() {
  var r2 = repl.start({
    input: stream,
    output: stream,
    terminal: false,
    useHistory: true
  });

  stream.write('blahblah');

  process.nextTick(function() {
    stream.emit('end');
  });

  r2.on('exit', function() {
    console.log('r2 exit')
    var contents = fs.readFileSync(process.env.NODE_HISTORY_PATH, 'utf8');
    fs.unlinkSync(process.env.NODE_HISTORY_PATH);
    assert.equal(contents, 'blahblah');
  });
}

testTerminalMode();
