'use strict';
const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');

if (cluster.isWorker) {

  // Just keep the worker alive
  process.send(process.argv[2]);

} else if (cluster.isMaster) {

  const checks = {
    args: false,
    setupEvent: false,
    settingsObject: false
  };

  const totalWorkers = 2;
  let settings;

  // Setup master
  cluster.setupMaster({
    args: ['custom argument'],
    silent: true
  });

  cluster.once('setup', function() {
    checks.setupEvent = true;

    settings = cluster.settings;
    if (settings &&
        settings.args && settings.args[0] === 'custom argument' &&
        settings.silent === true &&
        settings.exec === process.argv[1]) {
      checks.settingsObject = true;
    }
  });

  let correctInput = 0;

  cluster.on('online', common.mustCall(function listener(worker) {

    worker.once('message', function(data) {
      correctInput += (data === 'custom argument' ? 1 : 0);
      if (correctInput === totalWorkers) {
        checks.args = true;
      }
      worker.kill();
    });

  }, totalWorkers));

  // Start all workers
  cluster.fork();
  cluster.fork();

  // Check all values
  process.once('exit', function() {
    const argsMsg = 'Arguments was not send for one or more worker. ' +
                    `${correctInput} workers receive argument, ` +
                    `but ${totalWorkers} were expected.`;
    assert.ok(checks.args, argsMsg);

    assert.ok(checks.setupEvent, 'The setup event was never emitted');

    const settingObjectMsg = 'The settingsObject do not have correct ' +
                             `properties : ${JSON.stringify(settings)}`;
    assert.ok(checks.settingsObject, settingObjectMsg);
  });

}
