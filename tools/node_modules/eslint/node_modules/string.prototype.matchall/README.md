# string.prototype.matchall <sup>[![Version Badge][npm-version-svg]][package-url]</sup>

[![Build Status][travis-svg]][travis-url]
[![dependency status][deps-svg]][deps-url]
[![dev dependency status][dev-deps-svg]][dev-deps-url]
[![License][license-image]][license-url]
[![Downloads][downloads-image]][downloads-url]

[![npm badge][npm-badge-png]][package-url]

[![browser support][testling-svg]][testling-url]

ES Proposal spec-compliant shim for String.prototype.matchAll. Invoke its "shim" method to shim `String.prototype.matchAll` if it is unavailable or noncompliant.

This package implements the [es-shim API](https://github.com/es-shims/api) interface. It works in an ES3-supported environment, and complies with the [proposed spec](https://github.com/tc39/proposal-string-matchall).

Most common usage:
```js
const assert = require('assert');
const matchAll = require('string.prototype.matchall');

const str = 'aabc';
const nonRegexStr = 'ab';
const globalRegex = /[ac]/g;
const nonGlobalRegex = /[bc]/i;

// non-regex arguments are coerced into a global regex
assert.deepEqual(
	[...matchAll(str, nonRegexStr)],
	[...matchAll(str, new RegExp(nonRegexStr, 'g'))]
);

assert.deepEqual([...matchAll(str, globalRegex)], [
	Object.assign(['a'], { index: 0, input: str }),
	Object.assign(['a'], { index: 1, input: str }),
	Object.assign(['c'], { index: 3, input: str }),
]);

assert.deepEqual([...matchAll(str, nonGlobalRegex)], [
	Object.assign(['b'], { index: 2, input: str }),
]);

matchAll.shim(); // will be a no-op if not needed

// non-regex arguments are coerced into a global regex
assert.deepEqual(
	[...str.matchAll(nonRegexStr)],
	[...str.matchAll(new RegExp(nonRegexStr, 'g'))]
);

assert.deepEqual([...str.matchAll(globalRegex)], [
	Object.assign(['a'], { index: 0, input: str }),
	Object.assign(['a'], { index: 1, input: str }),
	Object.assign(['c'], { index: 3, input: str }),
]);

assert.deepEqual([...str.matchAll(nonGlobalRegex)], [
	Object.assign(['b'], { index: 2, input: str }),
]);

```

## Tests
Simply clone the repo, `npm install`, and run `npm test`

[package-url]: https://npmjs.com/package/string.prototype.matchall
[npm-version-svg]: http://versionbadg.es/es-shims/String.prototype.matchAll.svg
[travis-svg]: https://travis-ci.org/es-shims/String.prototype.matchAll.svg
[travis-url]: https://travis-ci.org/es-shims/String.prototype.matchAll
[deps-svg]: https://david-dm.org/es-shims/String.prototype.matchAll.svg
[deps-url]: https://david-dm.org/es-shims/String.prototype.matchAll
[dev-deps-svg]: https://david-dm.org/es-shims/String.prototype.matchAll/dev-status.svg
[dev-deps-url]: https://david-dm.org/es-shims/String.prototype.matchAll#info=devDependencies
[testling-svg]: https://ci.testling.com/es-shims/String.prototype.matchAll.png
[testling-url]: https://ci.testling.com/es-shims/String.prototype.matchAll
[npm-badge-png]: https://nodei.co/npm/string.prototype.matchall.png?downloads=true&stars=true
[license-image]: http://img.shields.io/npm/l/string.prototype.matchall.svg
[license-url]: LICENSE
[downloads-image]: http://img.shields.io/npm/dm/string.prototype.matchall.svg
[downloads-url]: http://npm-stat.com/charts.html?package=string.prototype.matchall
