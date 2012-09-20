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
var util = require('util');

if (os.type() != 'SunOS') {
  console.error('Skipping because postmortem debugging not available.');
  process.exit(0);
}

/*
 * Now we're going to fork ourselves to gcore
 */
var spawn = require('child_process').spawn;
var prefix = '/var/tmp/node';
var corefile = prefix + '.' + process.pid;
var gcore = spawn('gcore', [ '-o', prefix, process.pid + '' ]);
var output = '';
var unlinkSync = require('fs').unlinkSync;
var args = [ corefile ];

if (process.env.MDB_LIBRARY_PATH && process.env.MDB_LIBRARY_PATH != '')
  args = args.concat([ '-L', process.env.MDB_LIBRARY_PATH ]);

function LanguageH(chapter) { this.OBEY = 'CHAPTER ' + parseInt(chapter, 10); }
var obj = new LanguageH(1);

gcore.stderr.on('data', function (data) {
  console.log('gcore: ' + data);
});

gcore.on('exit', function (code) {
  if (code != 0) {
    console.error('gcore exited with code ' + code);
    process.exit(code);
  }

  var mdb = spawn('mdb', args, { stdio: 'pipe' });

  mdb.on('exit', function (code) {
    var retained = '; core retained as ' + corefile;

    if (code != 0) {
      console.error('mdb exited with code ' + util.inspect(code) + retained);
      process.exit(code);
    }

    var lines = output.split('\n');
    var found = 0, i, expected = 'OBEY: ' + obj.OBEY, nexpected = 2;

    for (var i = 0; i < lines.length; i++) {
      if (lines[i].indexOf(expected) != -1)
        found++;
    }

    assert.equal(found, nexpected, 'expected ' + nexpected +
      ' objects, found ' + found + retained);

    unlinkSync(corefile);
    process.exit(0);
  });

  mdb.stdout.on('data', function (data) {
    output += data;
  });

  mdb.stderr.on('data', function (data) {
    console.log('mdb stderr: ' + data);
  });

  mdb.stdin.write('::load v8.so\n');
  mdb.stdin.write('::findjsobjects -c LanguageH | ');
  mdb.stdin.write('::findjsobjects | ::jsprint\n');
  mdb.stdin.write('::findjsobjects -p OBEY | ');
  mdb.stdin.write('::findjsobjects | ::jsprint\n');
  mdb.stdin.end();
});

