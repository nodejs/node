```javascript
var assert = require('assert')
var spdxExceptions = require('spdx-exceptions')

assert(Array.isArray(spdxExceptions))

assert(spdxExceptions.length > 0)

function isString(x) {
  return typeof x === 'string' }

assert(spdxExceptions.every(isString))
```
