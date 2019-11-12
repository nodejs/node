'use strict';
const common = require('../common');
const assert = require('assert');
const domain = require('domain');

{
  const d = domain.create();
  d.run(common.mustCall(() => {
    const f = setTimeout(common.mustCall(() => {}), 1);
    assert.strictEqual(f.domain, d);
  }));

  Object.defineProperty(process, 'domain', {
    configurable: true,
    enumerable: true,
    get() { return null; },
    set(v) {}
  });

  d.run(common.mustCall(() => {
    const f = setTimeout(common.mustCall(() => {}), 1);
    assert.strictEqual(f.domain, undefined);
  }));
}
