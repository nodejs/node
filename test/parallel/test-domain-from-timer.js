'use strict';
// Simple tests of most basic domain functionality.

require('../common');
var assert = require('assert');

// timeouts call the callback directly from cc, so need to make sure the
// domain will be used regardless
setTimeout(function() {
  var domain = require('domain');
  var d = domain.create();
  d.run(function() {
    process.nextTick(function() {
      console.trace('in nexttick', process.domain === d);
      assert.equal(process.domain, d);
    });
  });
});
