// Flags: --expose-internals --no-async-context-frame
'use strict';

const common = require('../common');
const { Readable, finished } = require('stream');
const assert = require('assert');
const AsyncContextFrame = require('internal/async_context_frame');
const { enabledHooksExist } = require('internal/async_hooks');

// This test verifies that when there are no active async hooks, stream.finished() uses the default callback path

const readable = new Readable();

finished(readable, common.mustCall(() => {
  assert.strictEqual(enabledHooksExist(), false);
  assert.strictEqual(
    AsyncContextFrame.current() || enabledHooksExist(),
    false,
  );
}));

readable.destroy();
