'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI(['--port=0', fixtures.path('debugger/empty.js')]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('exec new Date(0)');
  assert.match(cli.output, /1970-01-01T00:00:00\.000Z/);
  await cli.command('exec null');
  assert.match(cli.output, /null/);
  await cli.command('exec /regex/g');
  assert.match(cli.output, /\/regex\/g/);
  await cli.command('exec new Map()');
  assert.match(cli.output, /Map\(0\) {}/);
  await cli.command('exec new Map([["a",1],["b",2]])');
  assert.match(cli.output, /Map\(2\) { a => 1, b => 2 }/);
  await cli.command('exec new Set()');
  assert.match(cli.output, /Set\(0\) {}/);
  await cli.command('exec new Set([1,2])');
  assert.match(cli.output, /Set\(2\) { 1, 2 }/);
  await cli.command('exec new Set([{a:1},new Set([1])])');
  assert.match(cli.output, /Set\(2\) { { a: 1 }, Set\(1\) { \.\.\. } }/);
  await cli.command('exec a={}; a');
  assert.match(cli.output, /{}/);
  await cli.command('exec a={a:1,b:{c:1}}; a');
  assert.match(cli.output, /{ a: 1, b: Object }/);
  await cli.command('exec a=[]; a');
  assert.match(cli.output, /\[\]/);
  await cli.command('exec a=[1,2]; a');
  assert.match(cli.output, /\[ 1, 2 \]/);
})()
.finally(() => cli.quit())
.then(common.mustCall());
