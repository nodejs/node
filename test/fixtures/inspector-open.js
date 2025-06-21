const assert = require('assert');
const inspector = require('inspector');


assert.strictEqual(inspector.url(), undefined);
inspector.open(0, undefined, false);
assert(inspector.url().startsWith('ws://'));
assert.throws(() => {
  inspector.open(0, undefined, false);
}, {
  code: 'ERR_INSPECTOR_ALREADY_ACTIVATED'
});
inspector.close();
assert.strictEqual(inspector.url(), undefined);
