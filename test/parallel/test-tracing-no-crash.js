'use strict';
const common = require('../common');
const { collectChildStreams } = require('../common/streams');
const assert = require('assert');
const { once } = require('events');
const { spawn } = require('child_process');

const child = spawn(process.execPath, [
  '--trace-event-categories', 'madeup', '-e', 'throw new Error()'
], { stdio: [ 'inherit', 'inherit', 'pipe' ] });

collectChildStreams(child, once(child, 'exit'))
  .then(common.mustCall(({ stderr, data: [code, signal] }) => {
    assert(stderr.includes('throw new Error()'), stderr);
    assert(!stderr.includes('Could not open trace file'), stderr);
    assert.strictEqual(signal, null);
    assert.strictEqual(code, 1);
  }));
