'use strict';

const common = require('../common');
const assert = require('assert');
const domain = require('domain');

{
  const d = new domain.Domain();

  const mustNotCall = common.mustNotCall();

  d.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'foobar');
    assert.strictEqual(err.domain, d);
    assert.strictEqual(err.domainEmitter, undefined);
    assert.strictEqual(err.domainBound, mustNotCall);
    assert.strictEqual(err.domainThrown, false);
  }));

  const bound = d.intercept(mustNotCall);
  bound(new Error('foobar'));
}

{
  const d = new domain.Domain();

  const bound = d.intercept(common.mustCall((data) => {
    assert.strictEqual(data, 'data');
  }));

  bound(null, 'data');
}

{
  const d = new domain.Domain();

  const bound = d.intercept(common.mustCall((data, data2) => {
    assert.strictEqual(data, 'data');
    assert.strictEqual(data2, 'data2');
  }));

  bound(null, 'data', 'data2');
}
