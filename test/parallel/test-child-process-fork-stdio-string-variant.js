'use strict';
const common = require('../common');

// Ensures that child_process.fork can accept string
// variant of stdio parameter in options object and
// throws a TypeError when given an unexpected string

const assert = require('assert');
const fork = require('child_process').fork;

const childScript = `${common.fixturesDir}/child-process-spawn-node`;
const errorRegexp = /^TypeError: Incorrect value of stdio option:/;
const malFormedOpts = {stdio: '33'};
const payload = {hello: 'world'};

assert.throws(() => fork(childScript, malFormedOpts), errorRegexp);

function test(stringVariant) {
  const child = fork(childScript, {stdio: stringVariant});

  child.on('message', common.mustCall((message) => {
    assert.deepStrictEqual(message, {foo: 'bar'});
  }));

  child.send(payload);

  child.on('exit', common.mustCall((code) => assert.strictEqual(code, 0)));
}

['pipe', 'inherit', 'ignore'].forEach(test);
