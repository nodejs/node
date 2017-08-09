'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

// These were picked arbitrarily and are only used to trigger arync_hooks.
const JSStream = process.binding('js_stream').JSStream;
const Socket = require('net').Socket;

const handle = new JSStream();
handle.domain = domain.create();
handle.domain.dispose();

handle.close = () => {};
handle.isAlive = () => { throw new Error(); };

const s = new Socket({ handle });

// When AsyncWrap::MakeCallback() returned an empty handle the
// MaybeLocal::ToLocalChecked() call caused the process to abort. By returning
// v8::Undefined() it allows the error to propagate to the 'error' event.
s.on('error', common.mustCall((e) => {
  assert.strictEqual(e.code, 'EINVAL');
}));
