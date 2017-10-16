'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const fs = require('fs');

{
  const d = new domain.Domain();

  d.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'foobar');
    assert.strictEqual(err.domain, d);
    assert.strictEqual(err.domainEmitter, undefined);
    assert.strictEqual(err.domainBound, undefined);
    assert.strictEqual(err.domainThrown, true);
  }));

  d.run(common.mustCall(() => {
    process.nextTick(common.mustCall(() => {
      const i = setInterval(common.mustCall(() => {
        clearInterval(i);
        setTimeout(common.mustCall(() => {
          fs.stat('this file does not exist', common.mustCall((er, stat) => {
            throw new Error('foobar');
          }));
        }), 1);
      }), 1);
    }));
  }));
}
