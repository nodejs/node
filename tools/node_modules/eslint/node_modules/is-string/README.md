# is-string <sup>[![Version Badge][2]][1]</sup>

[![Build Status][3]][4]
[![dependency status][5]][6]
[![dev dependency status][7]][8]
[![License][license-image]][license-url]
[![Downloads][downloads-image]][downloads-url]

[![npm badge][11]][1]

[![browser support][9]][10]

Is this value a JS String object or primitive? This module works cross-realm/iframe, and despite ES6 @@toStringTag.

## Example

```js
var isString = require('is-string');
var assert = require('assert');

assert.notOk(isString(undefined));
assert.notOk(isString(null));
assert.notOk(isString(false));
assert.notOk(isString(true));
assert.notOk(isString(function () {}));
assert.notOk(isString([]));
assert.notOk(isString({}));
assert.notOk(isString(/a/g));
assert.notOk(isString(new RegExp('a', 'g')));
assert.notOk(isString(new Date()));
assert.notOk(isString(42));
assert.notOk(isString(NaN));
assert.notOk(isString(Infinity));
assert.notOk(isString(new Number(42)));

assert.ok(isString('foo'));
assert.ok(isString(Object('foo')));
```

## Tests
Simply clone the repo, `npm install`, and run `npm test`

[1]: https://npmjs.org/package/is-string
[2]: http://versionbadg.es/ljharb/is-string.svg
[3]: https://travis-ci.org/ljharb/is-string.svg
[4]: https://travis-ci.org/ljharb/is-string
[5]: https://david-dm.org/ljharb/is-string.svg
[6]: https://david-dm.org/ljharb/is-string
[7]: https://david-dm.org/ljharb/is-string/dev-status.svg
[8]: https://david-dm.org/ljharb/is-string#info=devDependencies
[9]: https://ci.testling.com/ljharb/is-string.png
[10]: https://ci.testling.com/ljharb/is-string
[11]: https://nodei.co/npm/is-string.png?downloads=true&stars=true
[license-image]: http://img.shields.io/npm/l/is-string.svg
[license-url]: LICENSE
[downloads-image]: http://img.shields.io/npm/dm/is-string.svg
[downloads-url]: http://npm-stat.com/charts.html?package=is-string
