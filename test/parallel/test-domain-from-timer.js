'use strict';
// Simple tests of most basic domain functionality.

require('../common');
const assert = require('assert');

// timeouts call the callback directly from cc, so need to make sure the
// domain will be used regardless
setTimeout(function() {
  const domain = require('domain');
  const d = domain.create();
  d.run(function() {
    process.nextTick(function() {
      console.trace('in nexttick', process.domain === d);
      assert.strictEqual(process.domain, d);
    });
  });
}, 1);
