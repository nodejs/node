'use strict';
const common = require('../common');

const assert = require('assert');
const fork = require('child_process').fork;

const childScript = `${common.fixturesDir}/child-process-spawn-node`;
const payload = {hello: 'world'};
const stringOpts = {stdio: 'ignore'};

const child = fork(childScript, stringOpts);

child.on('message', (message) => {
  assert.deepStrictEqual(message, {foo: 'bar'});
});

child.send(payload);

child.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
