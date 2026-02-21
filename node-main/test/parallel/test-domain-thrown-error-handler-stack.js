'use strict';

const common = require('../common');
const domain = require('domain');

// Make sure that when an error is thrown from a nested domain, its error
// handler runs outside of that domain, but within the context of any parent
// domain.

const d = domain.create();
const d2 = domain.create();

d2.on('error', common.mustCall((err) => {
  if (domain._stack.length !== 1) {
    console.error('domains stack length should be 1 but is %d',
                  domain._stack.length);
    process.exit(1);
  }

  if (process.domain !== d) {
    console.error('active domain should be %j but is %j', d, process.domain);
    process.exit(1);
  }

  process.nextTick(() => {
    if (domain._stack.length !== 1) {
      console.error('domains stack length should be 1 but is %d',
                    domain._stack.length);
      process.exit(1);
    }

    if (process.domain !== d) {
      console.error('active domain should be %j but is %j', d,
                    process.domain);
      process.exit(1);
    }
  });
}));

d.run(() => {
  d2.run(() => {
    throw new Error('oops');
  });
});
