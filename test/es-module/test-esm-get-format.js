require('../common');
const assert = require('assert');
const { defaultGetFormat} = require('internal/modules/esm/get_format');

{
  const url = new URL('file://example.com/foo/bar.js')
  assert.strictEqual(defaultGetFormat(url), 'commonjs')
}

{
  const url = new URL('file://example.com/foo/bar.whatever')
  assert.throws(() => defaultGetFormat(url), {name: 'TypeError', message: /Unknown file extension whatever/})
}
