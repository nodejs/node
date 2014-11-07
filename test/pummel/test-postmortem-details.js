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
 * We're going to look specifically for this function and buffer in the core
 * file.
 */
function myTestFunction()
{
  [ 1 ].forEach(function myIterFunction(t) {});
  return (new Buffer(bufstr));
}

var bufstr, mybuffer;

/*
 * Run myTestFunction() three times to create multiple instances of
 * myIterFunction.
 */
bufstr = 'Hello, test suite!';
mybuffer = myTestFunction(bufstr);
mybuffer = myTestFunction(bufstr);
mybuffer = myTestFunction(bufstr);
mybuffer.my_buffer = true;

var OBJECT_KINDS = ['dict', 'inobject', 'numeric', 'props'];
var NODE_VERSION = common.getNodeVersion();

/*
 * Now we're going to fork ourselves to gcore
 */
var spawn = require('child_process').spawn;
var prefix = '/var/tmp/node';
var corefile = prefix + '.' + process.pid;
var tmpfile = '/var/tmp/node-postmortem-func' + '.' + process.pid;
var gcore = spawn('gcore', [ '-o', prefix, process.pid + '' ]);
var output = '';
var unlinkSync = require('fs').unlinkSync;
var args = [ corefile ];
var mybuffer;

if (process.env.MDB_LIBRARY_PATH && process.env.MDB_LIBRARY_PATH != '')
  args = args.concat([ '-L', process.env.MDB_LIBRARY_PATH ]);

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
    unlinkSync(tmpfile);
    var retained = '; core retained as ' + corefile;

    if (code != 0) {
      console.error('mdb exited with code ' + util.inspect(code) + retained);
      process.exit(code);
    }

    var lines = output.split(/\n/);
    var current = null, testname = null;
    var whichtest = -1;
    var i;

    for (i = 0; i < lines.length; i++) {
      if (lines[i].indexOf('test: ') === 0) {
        if (current !== null) {
          console.error('verifying ' + testname + ' using ' +
              verifiers[whichtest].name);
          verifiers[whichtest](current);
        }
        whichtest++;
        current = [];
        testname = lines[i];
        continue;
      }

      if (current !== null)
        current.push(lines[i]);
    }

    console.error('verifying ' + testname + ' using ' +
        verifiers[whichtest].name);
    verifiers[whichtest](current);

    unlinkSync(corefile);
    process.exit(0);
  });

  mdb.stdout.on('data', function (data) {
    output += data;
  });

  mdb.stderr.on('data', function (data) {
    console.log('mdb stderr: ' + data);
  });

  var verifiers = [];
  var buffer;
  verifiers.push(function verifyConstructor(testlines) {
    assert.deepEqual(testlines, [ 'Buffer' ]);
  });
  verifiers.push(function verifyNodebuffer(testlines) {
    assert.equal(testlines.length, 1);
    assert.ok(/^[0-9a-fA-F]+$/.test(testlines[0]));
    buffer = testlines[0];
  });
  verifiers.push(function verifyBufferContents(testlines) {
    assert.equal(testlines.length, 1);
    assert.equal(testlines[0], '0x' + buffer + ':      Hello');
  });
  verifiers.push(function verifyV8internal(testlines) {
    assert.deepEqual(testlines, [ buffer ]);
  });
  verifiers.push(function verifyJsfunctionN(testlines) {
    assert.equal(testlines.length, 2);
    var parts = testlines[1].trim().split(/\s+/);
    assert.equal(parts[1], 1);
    assert.equal(parts[2], 'myTestFunction');
    assert.ok(parts[3].indexOf('test-postmortem-details.js') != -1);
  });
  verifiers.push(function verifyJsfunctionS(testlines) {
    var foundtest = false, founditer = false;
    assert.ok(testlines.length > 1);
    testlines.forEach(function (line) {
      var parts = line.trim().split(/\s+/);
      if (parts[2] == 'myIterFunction') {
        assert.equal(parts[1], '3');
        founditer = true;
      } else if (parts[2] == 'myTestFunction') {
        foundtest = true;
        assert.equal(parts[1], '1');
      }
    });
    assert.ok(foundtest);
    assert.ok(founditer);
  });
  verifiers.push(function verifyJssource(testlines) {
    var content = testlines.join('\n');
    assert.ok(testlines[0].indexOf('test-postmortem-details.js') != -1);
    assert.ok(content.indexOf('function myTestFunction()\n') != -1);
    assert.ok(content.indexOf('return (new Buffer(bufstr));\n') != -1);
  });
  OBJECT_KINDS.forEach(function (kind) {
    verifiers.push(function verifyFindObjectsKind(testLines) {
      // There should be at least one object for
      // every kind of objects (except for the special cases
      // below)
      var expectedMinimumObjs = 1;

      if (kind === 'props' &&
          (NODE_VERSION.major > 0 || NODE_VERSION.minor > 10)) {
        // On versions > 0.10.x, currently there's no object
        // with the kind 'props'. There should be, but it's a minor
        // issue we're or to live with for now.
        expectedMinimumObjs = 0;
      }

      assert.ok(testLines.length >= expectedMinimumObjs);
    });
  });

  var mod = util.format('::load %s\n',
                        path.join(__dirname,
                                  '..',
                                  '..',
                                  'out',
                                  'Release',
                                  'mdb_v8.so'));
  mdb.stdin.write(mod);
  mdb.stdin.write('!echo test: jsconstructor\n');
  mdb.stdin.write('::findjsobjects -p my_buffer | ::findjsobjects | ' +
      '::jsprint -b length ! awk -F: \'$2 == ' + bufstr.length + '{ print $1 }\'' +
      '| head -1 > ' + tmpfile + '\n');
  mdb.stdin.write('::cat ' + tmpfile + ' | ::jsconstructor\n');
  mdb.stdin.write('!echo test: nodebuffer\n');
  mdb.stdin.write('::cat ' + tmpfile + ' | ::nodebuffer\n');
  mdb.stdin.write('!echo test: nodebuffer contents\n');
  mdb.stdin.write('::cat ' + tmpfile + ' | ::nodebuffer | ::eval "./ccccc"\n');

  mdb.stdin.write('!echo test: v8internal\n');
  mdb.stdin.write('::cat ' + tmpfile + ' | ::v8print ! awk \'$2 == "elements"{' +
      'print $4 }\' > ' + tmpfile + '\n');
  mdb.stdin.write('::cat ' + tmpfile + ' | ::v8internal 0\n');

  mdb.stdin.write('!echo test: jsfunctions -n\n');
  mdb.stdin.write('::jsfunctions -n myTestFunction ! cat\n');
  mdb.stdin.write('!echo test: jsfunctions -s\n');
  mdb.stdin.write('::jsfunctions -s test-postmortem-details.js ! cat\n');
  mdb.stdin.write('!echo test: jssource\n');
  mdb.stdin.write('::jsfunctions -n myTestFunction ! ' +
      'awk \'NR == 2 {print $1}\' | head -1 > ' + tmpfile + '\n');
  mdb.stdin.write('::cat ' + tmpfile + ' | ::jssource -n 0\n');
  OBJECT_KINDS.forEach(function (kind) {
    mdb.stdin.write(util.format('!echo test: findjsobjects -k %s\n', kind));
    mdb.stdin.write(util.format('::findjsobjects -k %s\n', kind));
  });
  mdb.stdin.end();
});
