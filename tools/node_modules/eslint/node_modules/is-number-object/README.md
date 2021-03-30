# is-number-object <sup>[![Version Badge][2]][1]</sup>

[![Build Status][3]][4]
[![dependency status][5]][6]
[![dev dependency status][7]][8]
[![License][license-image]][license-url]
[![Downloads][downloads-image]][downloads-url]

[![npm badge][11]][1]

Is this value a JS Number object? This module works cross-realm/iframe, and despite ES6 @@toStringTag.

## Example

```js
var isNumber = require('is-number-object');
var assert = require('assert');

assert.notOk(isNumber(undefined));
assert.notOk(isNumber(null));
assert.notOk(isNumber(false));
assert.notOk(isNumber(true));
assert.notOk(isNumber('foo'));
assert.notOk(isNumber(function () {}));
assert.notOk(isNumber([]));
assert.notOk(isNumber({}));
assert.notOk(isNumber(/a/g));
assert.notOk(isNumber(new RegExp('a', 'g')));
assert.notOk(isNumber(new Date()));

assert.ok(isNumber(42));
assert.ok(isNumber(NaN));
assert.ok(isNumber(Infinity));
assert.ok(isNumber(new Number(42)));
```

## Tests
Simply clone the repo, `npm install`, and run `npm test`

[1]: https://npmjs.org/package/is-number-object
[2]: http://versionbadg.es/inspect-js/is-number-object.svg
[3]: https://travis-ci.org/inspect-js/is-number-object.svg
[4]: https://travis-ci.org/inspect-js/is-number-object
[5]: https://david-dm.org/inspect-js/is-number-object.svg
[6]: https://david-dm.org/inspect-js/is-number-object
[7]: https://david-dm.org/inspect-js/is-number-object/dev-status.svg
[8]: https://david-dm.org/inspect-js/is-number-object#info=devDependencies
[11]: https://nodei.co/npm/is-number-object.png?downloads=true&stars=true
[license-image]: http://img.shields.io/npm/l/is-number-object.svg
[license-url]: LICENSE
[downloads-image]: http://img.shields.io/npm/dm/is-number-object.svg
[downloads-url]: http://npm-stat.com/charts.html?package=is-number-object
