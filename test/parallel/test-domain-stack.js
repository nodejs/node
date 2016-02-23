'use strict';
// Make sure that the domain stack doesn't get out of hand.

require('../common');
var domain = require('domain');

var a = domain.create();
a.name = 'a';

a.on('error', function() {
  if (domain._stack.length > 5) {
    console.error('leaking!', domain._stack);
    process.exit(1);
  }
});

var foo = a.bind(function() {
  throw new Error('error from foo');
});

for (var i = 0; i < 1000; i++) {
  process.nextTick(foo);
}

process.on('exit', function(c) {
  if (!c) console.log('ok');
});
