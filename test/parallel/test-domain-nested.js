'use strict';
// Make sure that the nested domains don't cause the domain stack to grow

require('../common');
var assert = require('assert');
var domain = require('domain');

process.on('exit', function(c) {
  assert.equal(domain._stack.length, 0);
});

domain.create().run(function() {
  domain.create().run(function() {
    domain.create().run(function() {
      domain.create().on('error', function(e) {
          // Don't need to do anything here
      }).run(function() {
        throw new Error('died');
      });
    });
  });
});
