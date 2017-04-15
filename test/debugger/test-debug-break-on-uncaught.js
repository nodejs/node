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

'use strict';
const common = require('../common');
const path = require('path');
const assert = require('assert');
const spawn = require('child_process').spawn;
const debug = require('_debugger');

const scenarios = [];

addScenario('global.js', 2);
addScenario('timeout.js', 2);

run();

/***************** IMPLEMENTATION *****************/

function addScenario(scriptName, throwsOnLine) {
  scenarios.push(
    runScenario.bind(null, scriptName, throwsOnLine, run)
  );
}

function run() {
  const next = scenarios.shift();
  if (next) next();
}

function runScenario(scriptName, throwsOnLine, next) {
  let asserted = false;
  const port = common.PORT;

  const testScript = path.join(
    common.fixturesDir,
    'uncaught-exceptions',
    scriptName
  );

  const child = spawn(process.execPath, [ '--debug-brk=' + port, testScript ]);
  child.on('close', function() {
    assert(asserted, 'debugger did not pause on exception');
    if (next) next();
  });

  const exceptions = [];

  let stderr = '';

  function stderrListener(data) {
    stderr += data;
    if (stderr.includes('Debugger listening on ')) {
      setTimeout(setupClient.bind(null, runTest), 200);
      child.stderr.removeListener('data', stderrListener);
    }
  }

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', stderrListener);

  function setupClient(callback) {
    const client = new debug.Client();

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

  let interval;
  function runTest(client) {
    client.req(
      {
        command: 'setexceptionbreak',
        arguments: {
          type: 'uncaught',
          enabled: true
        }
      },
      function(error) {
        assert.ifError(error);

        client.on('exception', function(event) {
          exceptions.push(event.body);
        });

        client.reqContinue(function(error) {
          assert.ifError(error);
          interval = setInterval(assertHasPaused.bind(null, client), 10);
        });
      }
    );
  }

  function assertHasPaused(client) {
    if (!exceptions.length) return;

    assert.strictEqual(exceptions.length, 1,
                       'debugger did not pause on exception');
    assert.strictEqual(exceptions[0].uncaught, true);
    assert.strictEqual(exceptions[0].script.name, testScript);
    assert.strictEqual(exceptions[0].sourceLine + 1, throwsOnLine);
    asserted = true;
    client.reqContinue(assert.ifError);
    clearInterval(interval);
  }
}
