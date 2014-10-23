// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common');
var assert = require('assert');
var os = require('os');
var path = require('path');
var util = require('util');

if (os.type() != 'SunOS') {
  console.error('Skipping because postmortem debugging not available.');
  process.exit(0);
}

/*
 * Some functions to create a recognizable stack.
 */
var frames = [ 'stalloogle', 'bagnoogle', 'doogle' ];
var expected;

var stalloogle = function (str) {
  expected = str;
  os.loadavg();
};

var bagnoogle = function (arg0, arg1) {
  stalloogle(arg0 + ' is ' + arg1 + ' except that it is read-only');
};

var done = false;

var doogle = function () {
  if (!done)
    setTimeout(doogle, 10);

  bagnoogle('The bfs command', '(almost) like ed(1)');
};

var spawn = require('child_process').spawn;
var prefix = '/var/tmp/node';
var corefile = prefix + '.' + process.pid;
var args = [ corefile ];

if (process.env.MDB_LIBRARY_PATH && process.env.MDB_LIBRARY_PATH != '')
  args = args.concat([ '-L', process.env.MDB_LIBRARY_PATH ]);

/*
 * We're going to use DTrace to stop us, gcore us, and set us running again
 * when we call getloadavg() -- with the implicit assumption that our
 * deepest function is the only caller of os.loadavg().
 */
var dtrace = spawn('dtrace', [ '-qwn', 'syscall::getloadavg:entry/pid == ' +
  process.pid + '/{stop(); system("gcore -o ' +
  prefix + ' %d", pid); system("prun %d", pid); exit(0); }' ]);

var output = '';
var unlinkSync = require('fs').unlinkSync;

dtrace.stderr.on('data', function (data) {
  console.log('dtrace: ' + data);
});

dtrace.on('exit', function (code) {
  if (code != 0) {
    console.error('dtrace exited with code ' + code);
    process.exit(code);
  }

  done = true;

  /*
   * We have our core file.  Now we need to fire up mdb to analyze it...
   */
  var mdb = spawn('mdb', args, { stdio: 'pipe' });

  var mod = util.format('::load %s\n',
                        path.join(__dirname,
                                  '..',
                                  '..',
                                  'out',
                                  'Release',
                                  'mdb_v8.so'));

  mdb.on('exit', function (code) {
    var retained = '; core retained as ' + corefile;

    if (code != 0) {
      console.error('mdb exited with code ' + code + retained);
      process.exit(code);
    }

    var sentinel = '<anonymous> (as ';
    var arg1 = '          arg1: ';
    var lines = output.split('\n');
    var matched = 0;
    var straddr = undefined;

    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];

      if (matched == 1 && line.indexOf(arg1) === 0) {
        straddr = line.substr(arg1.length).split(' ')[0];
      }

      if (line.indexOf(sentinel) == -1 || frames.length === 0)
        continue;

      var frame = line.substr(line.indexOf(sentinel) + sentinel.length);
      var top = frames.shift();

      assert.equal(frame.indexOf(top), 0, 'unexpected frame where ' +
        top + ' was expected' + retained);

      matched++;
    }

    assert.equal(frames.length, 0, 'did not find expected frame ' +
      frames[0] + retained);

    assert.notEqual(straddr, undefined,
      'did not find arg1 for top frame' + retained);

    /*
     * Now we're going to take one more swing at the core file to print out
     * the argument string that we found.
     */
    output = '';
    mdb = spawn('mdb', args, { stdio: 'pipe' });

    mdb.on('exit', function (code) {
      if (code != 0) {
        console.error('mdb (second) exited with code ' + code + retained);
        process.exit(code);
      }

      assert.notEqual(output.indexOf(expected), -1, 'did not find arg1 (' +
        straddr + ') to contain expected string' + retained);

      unlinkSync(corefile);
      process.exit(0);
    });

    mdb.stdout.on('data', function (data) {
      output += data;
    });

    mdb.stderr.on('data', function (data) {
      console.log('mdb (second) stderr: ' + data);
    });

    mdb.stdin.write(mod);
    mdb.stdin.write(straddr + '::v8str\n');
    mdb.stdin.end();
  });

  mdb.stdout.on('data', function (data) {
    output += data;
  });

  mdb.stderr.on('data', function (data) {
    console.log('mdb stderr: ' + data);
  });

  mdb.stdin.write(mod);
  mdb.stdin.write('::jsstack -v\n');
  mdb.stdin.end();
});

setTimeout(doogle, 10);

