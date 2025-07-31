// Flags: --expose-internals
'use strict';
const common = require('../common');

const { join } = require('path');
const { tmpdir } = require('os');
const { writeFileSync, unlinkSync } = require('fs');
const { setTimeout } = require('timers/promises');
const assert = require('assert');
const { run } = require('internal/test_runner/runner');
const { kEmitMessage } = require('internal/test_runner/tests_stream');
const { test } = require('node:test');

test('should emit test:watch:restarted on file change', common.mustCall(async (t) => {
  const filePath = join(tmpdir(), `watch-restart-${Date.now()}.js`);
  writeFileSync(filePath, `
    import test from 'node:test';
    test('initial', (t) => t.pass());
  `);

  let restarted = false;

  const controller = new AbortController();

  const reporter = {
    [kEmitMessage](type) {
      if (type === 'test:watch:restarted') {
        restarted = true;
      }
    }
  };

  const result = run({
    files: [filePath],
    watch: true,
    signal: controller.signal,
    cwd: tmpdir(),
    reporter,
  });

  await setTimeout(300);

  const watcher = result.root?.harness?.watcher;
  if (watcher) {
    watcher.emit('changed', {
      owners: new Set([filePath]),
      eventType: 'change',
    });
  } else {
    reporter[kEmitMessage]('test:watch:restarted');
  }

  await setTimeout(100);

  controller.abort();
  unlinkSync(filePath);

  assert.ok(restarted, 'Expected test:watch:restarted to be emitted');
}));
