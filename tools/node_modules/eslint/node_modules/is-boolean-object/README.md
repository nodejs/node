# is-boolean-object <sup>[![Version Badge][2]][1]</sup>

[![Build Status][3]][4]
[![dependency status][5]][6]
[![dev dependency status][7]][8]
[![License][license-image]][license-url]
[![Downloads][downloads-image]][downloads-url]

[![npm badge][11]][1]

[![browser support][9]][10]

Is this value a JS Boolean? This module works cross-realm/iframe, and despite ES6 @@toStringTag.

## Example

```js
var isBoolean = require('is-boolean-object');
var assert = require('assert');

assert.notOk(isBoolean(undefined));
assert.notOk(isBoolean(null));
assert.notOk(isBoolean('foo'));
assert.notOk(isBoolean(function () {}));
assert.notOk(isBoolean([]));
assert.notOk(isBoolean({}));
assert.notOk(isBoolean(/a/g));
assert.notOk(isBoolean(new RegExp('a', 'g')));
assert.notOk(isBoolean(new Date()));
assert.notOk(isBoolean(42));
assert.notOk(isBoolean(NaN));
assert.notOk(isBoolean(Infinity));

assert.ok(isBoolean(new Boolean(42)));
assert.ok(isBoolean(false));
assert.ok(isBoolean(Object(false)));
assert.ok(isBoolean(true));
assert.ok(isBoolean(Object(true)));
```

## Tests
Simply clone the repo, `npm install`, and run `npm test`

[1]: https://npmjs.org/package/is-boolean-object
[2]: http://versionbadg.es/ljharb/is-boolean-object.svg
[3]: https://travis-ci.org/ljharb/is-boolean-object.svg
[4]: https://travis-ci.org/ljharb/is-boolean-object
[5]: https://david-dm.org/ljharb/is-boolean-object.svg
[6]: https://david-dm.org/ljharb/is-boolean-object
[7]: https://david-dm.org/ljharb/is-boolean-object/dev-status.svg
[8]: https://david-dm.org/ljharb/is-boolean-object#info=devDependencies
[9]: https://ci.testling.com/ljharb/is-boolean-object.png
[10]: https://ci.testling.com/ljharb/is-boolean-object
[11]: https://nodei.co/npm/is-boolean-object.png?downloads=true&stars=true
[license-image]: http://img.shields.io/npm/l/is-boolean-object.svg
[license-url]: LICENSE
[downloads-image]: http://img.shields.io/npm/dm/is-boolean-object.svg
[downloads-url]: http://npm-stat.com/charts.html?package=is-boolean-object
