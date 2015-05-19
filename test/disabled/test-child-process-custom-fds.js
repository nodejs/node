'use strict';
var common = require('../common');
var assert = require('assert');

var assert = require('assert');
var spawn = require('child_process').spawn;
var path = require('path');
var fs = require('fs');

function fixtPath(p) {
  return path.join(common.fixturesDir, p);
}

var expected = 'hello world';

// Test the equivalent of:
// $ /bin/echo 'hello world' > hello.txt
var helloPath = path.join(common.tmpDir, 'hello.txt');

function test1(next) {
  console.log('Test 1...');
  fs.open(helloPath, 'w', 400, function(err, fd) {
    if (err) throw err;
    var child = spawn('/bin/echo', [expected], {customFds: [-1, fd]});

    assert.notEqual(child.stdin, null);
    assert.equal(child.stdout, null);
    assert.notEqual(child.stderr, null);

    child.on('exit', function(err) {
      if (err) throw err;
      fs.close(fd, function(error) {
        if (error) throw error;

        fs.readFile(helloPath, function(err, data) {
          if (err) throw err;
          assert.equal(data.toString(), expected + '\n');
          console.log('  File was written.');
          next(test3);
        });
      });
    });
  });
}

// Test the equivalent of:
// $ node ../fixture/stdio-filter.js < hello.txt
function test2(next) {
  console.log('Test 2...');
  fs.open(helloPath, 'r', undefined, function(err, fd) {
    var child = spawn(process.argv[0],
                      [fixtPath('stdio-filter.js'), 'o', 'a'],
                      {customFds: [fd, -1, -1]});

    assert.equal(child.stdin, null);
    var actualData = '';
    child.stdout.on('data', function(data) {
      actualData += data.toString();
    });
    child.on('exit', function(code) {
      if (err) throw err;
      assert.equal(actualData, 'hella warld\n');
      console.log('  File was filtered successfully');
      fs.close(fd, function() {
        next(test3);
      });
    });
  });
}

// Test the equivalent of:
// $ /bin/echo 'hello world' | ../stdio-filter.js a o
function test3(next) {
  console.log('Test 3...');
  var filter = spawn(process.argv[0], [fixtPath('stdio-filter.js'), 'o', 'a']);
  var echo = spawn('/bin/echo', [expected], {customFds: [-1, filter.fds[0]]});
  var actualData = '';
  filter.stdout.on('data', function(data) {
    console.log('  Got data --> ' + data);
    actualData += data;
  });
  filter.on('exit', function(code) {
    if (code) throw 'Return code was ' + code;
    assert.equal(actualData, 'hella warld\n');
    console.log('  Talked to another process successfully');
  });
  echo.on('exit', function(code) {
    if (code) throw 'Return code was ' + code;
    filter.stdin.end();
    fs.unlinkSync(helloPath);
  });
}

test1(test2);
