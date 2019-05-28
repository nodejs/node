'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');

const fs = require('fs');
const assert = require('assert');
const cluster = require('cluster');

const waitForEvent = (eventEmiter, event) =>
  new Promise((resolve) => eventEmiter.once(event, resolve));

function findLogFileWithName(name, dir) {
  const data = fs.readdirSync(dir, 'utf8');
  return data.filter((file) => file.includes(name));
}

async function shouldFillLogfileArg() {
  tmpdir.refresh();

  cluster.setupMaster({
    exec: __dirname,
    execArgv: ['--prof', '--logfile'],
    cwd: tmpdir.path
  });

  cluster.fork();

  cluster.once('exit', common.mustCall((worker) => {
    const EXPECTED_FILENAME = 'v8-' + worker.process.pid;
    const reports = findLogFileWithName(EXPECTED_FILENAME, tmpdir.path);
    tmpdir.refresh();
    assert.strictEqual(reports.length, 1);
  }));

  await waitForEvent(cluster, 'exit');
}

async function shouldCreateLogWithArgName() {
  tmpdir.refresh();

  const EXPECTED_FILENAME = 'test';

  cluster.setupMaster({
    exec: __dirname,
    execArgv: ['--prof', '--logfile=' + EXPECTED_FILENAME],
    cwd: tmpdir.path
  });

  cluster.fork();

  cluster.once('exit', common.mustCall((worker) => {
    const reports = findLogFileWithName(EXPECTED_FILENAME, tmpdir.path);
    tmpdir.refresh();
    assert.strictEqual(reports.length, 1);
  }));

  await waitForEvent(cluster, 'exit');
}

if (cluster.isMaster) {
  (async () => {
    await shouldFillLogfileArg();
    await shouldCreateLogWithArgName();
  })();
}
