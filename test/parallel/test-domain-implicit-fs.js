'use strict';
// Simple tests of most basic domain functionality.

const common = require('../common');
var assert = require('assert');
var domain = require('domain');

var d = new domain.Domain();

d.on('error', common.mustCall(function(er) {
  console.error('caught', er);

  assert.strictEqual(er.domain, d);
  assert.strictEqual(er.domainThrown, true);
  assert.ok(!er.domainEmitter);
  assert.strictEqual(er.code, 'ENOENT');
  assert.ok(/\bthis file does not exist\b/i.test(er.path));
  assert.strictEqual(typeof er.errno, 'number');
}));


// implicit handling of thrown errors while in a domain, via the
// single entry points of ReqWrap and MakeCallback.  Even if
// we try very hard to escape, there should be no way to, even if
// we go many levels deep through timeouts and multiple IO calls.
// Everything that happens between the domain.enter() and domain.exit()
// calls will be bound to the domain, even if multiple levels of
// handles are created.
d.run(function() {
  setTimeout(function() {
    var fs = require('fs');
    fs.readdir(__dirname, function() {
      fs.open('this file does not exist', 'r', function(er) {
        if (er) throw er;
        throw new Error('should not get here!');
      });
    });
  }, 100);
});
