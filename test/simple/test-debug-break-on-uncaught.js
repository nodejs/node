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

var path = require('path');
var assert = require('assert');
var spawn = require('child_process').spawn;
var common = require('../common');
var debug = require('_debugger');

addScenario('global.js', null, 2);
addScenario('timeout.js', null, 2);
addScenario('domain.js', null, 10);

// Exception is thrown from vm.js via module.js (internal file)
//   var compiledWrapper = runInThisContext(wrapper, filename, 0, true);
addScenario('parse-error.js', 'vm.js', null);

run();

/***************** IMPLEMENTATION *****************/

var scenarios;
function addScenario(scriptName, throwsInFile, throwsOnLine) {
  if (!scenarios) scenarios = [];
  scenarios.push(
    runScenario.bind(null, scriptName, throwsInFile, throwsOnLine, run)
  );
}

function run() {
  var next = scenarios.shift();
  if (next) next();
}

function runScenario(scriptName, throwsInFile, throwsOnLine, next) {
  console.log('**[ %s ]**', scriptName);
  var asserted = false;
  var port = common.PORT + 1337;

  var testScript = path.join(
    common.fixturesDir,
    'uncaught-exceptions',
    scriptName
  );

  var child = spawn(process.execPath, [ '--debug-brk=' + port, testScript ]);
  child.on('close', function() {
    assert(asserted, 'debugger did not pause on exception');
    if (next) next();
  })

  var exceptions = [];

  setTimeout(setupClient.bind(null, runTest), 200);

  function setupClient(callback) {
    var client = new debug.Client();

    client.once('ready', callback.bind(null, client));

    client.on('unhandledResponse', function(body) {
      console.error('unhandled response: %j', body);
    });

    client.on('error', function(err) {
      if (asserted) return;
      assert.ifError(err);
    });

    client.connect(port);
  }

  function runTest(client) {
    client.req(
      {
        command: 'setexceptionbreak',
        arguments: {
          type: 'uncaught',
          enabled: true
        }
      },
      function(error, result) {
        assert.ifError(error);

        client.on('exception', function(event) {
          exceptions.push(event.body);
        });

        client.reqContinue(function(error, result) {
          assert.ifError(error);
          setTimeout(assertHasPaused.bind(null, client), 100);
        });
      }
    );
  }

  function assertHasPaused(client) {
    assert.equal(exceptions.length, 1, 'debugger did not pause on exception');
    assert.equal(exceptions[0].uncaught, true);
    assert.equal(exceptions[0].script.name, throwsInFile || testScript);
    if (throwsOnLine != null)
      assert.equal(exceptions[0].sourceLine + 1, throwsOnLine);
    asserted = true;
    client.reqContinue(assert.ifError);
  }
}
