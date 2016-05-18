```javascript
var parse = require('spdx-expression-parse')
var assert = require('assert')

var firstAST = {
  left: { license: 'LGPL-2.1' },
  conjunction: 'or',
  right: {
    left: { license: 'BSD-3-Clause' },
    conjunction: 'and',
    right: { license: 'MIT' } } }

assert.deepEqual(
  parse('(LGPL-2.1 OR BSD-3-Clause AND MIT)'),
  firstAST)

var secondAST = {
  left: { license: 'MIT' },
  conjunction: 'and',
  right: {
    left: {
	  license: 'LGPL-2.1',
	  plus: true },
    conjunction: 'and',
    right: { license: 'BSD-3-Clause' } } }

assert.deepEqual(
  parse('(MIT AND (LGPL-2.1+ AND BSD-3-Clause))'),
  secondAST)

// We handle all the bare SPDX license and exception ids as well.
require('spdx-license-ids').forEach(function(id) {
  assert.deepEqual(
    parse(id),
    { license: id })
  require('spdx-exceptions').forEach(function(e) {
    assert.deepEqual(
      parse(id + ' WITH ' + e),
      { license: id, exception: e }) }) })
```

---

[The Software Package Data Exchange (SPDX) specification](http://spdx.org) is the work of the [Linux Foundation](http://www.linuxfoundation.org) and its contributors, and is licensed under the terms of [the Creative Commons Attribution License 3.0 Unported (SPDX: "CC-BY-3.0")](http://spdx.org/licenses/CC-BY-3.0). "SPDX" is a United States federally registered trademark of the Linux Foundation.
