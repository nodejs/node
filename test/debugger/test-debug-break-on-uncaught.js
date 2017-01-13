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
          setTimeout(assertHasPaused.bind(null, client), 100);
        });
      }
    );
  }

  function assertHasPaused(client) {
    assert(exceptions.length, 'no exceptions thrown, race condition in test?');
    assert.strictEqual(exceptions.length, 1,
                       'debugger did not pause on exception');
    assert.strictEqual(exceptions[0].uncaught, true);
    assert.strictEqual(exceptions[0].script.name, testScript);
    assert.strictEqual(exceptions[0].sourceLine + 1, throwsOnLine);
    asserted = true;
    client.reqContinue(assert.ifError);
  }
}
