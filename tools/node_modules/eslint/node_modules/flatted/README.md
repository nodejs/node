# flatted

![Downloads](https://img.shields.io/npm/dm/flatted.svg) [![Coverage Status](https://coveralls.io/repos/github/WebReflection/flatted/badge.svg?branch=master)](https://coveralls.io/github/WebReflection/flatted?branch=master) [![Build Status](https://travis-ci.org/WebReflection/flatted.svg?branch=master)](https://travis-ci.org/WebReflection/flatted) [![License: ISC](https://img.shields.io/badge/License-ISC-yellow.svg)](https://opensource.org/licenses/ISC) ![WebReflection status](https://offline.report/status/webreflection.svg)

A super light (0.5K) and fast circular JSON parser, directly from the creator of [CircularJSON](https://github.com/WebReflection/circular-json/#circularjson).

```js
npm i flatted
```

Usable via [CDN](https://unpkg.com/flatted) or as regular module.

```js
// ESM
import {parse, stringify} from 'flatted/esm';

// CJS
const {parse, stringify} = require('flatted/cjs');

const a = [{}];
a[0].a = a;
a.push(a);

stringify(a); // [["1","0"],{"a":"0"}]
```

## Flatted VS JSON

As it is for every other specialized format capable of serializing and deserializing circular data, you should never `JSON.parse(Flatted.stringify(data))`, and you should never `Flatted.parse(JSON.stringify(data))`.

The only way this could work is to `Flatted.parse(Flatted.stringify(data))`, as it is also for _CircularJSON_ or any other, otherwise there's no granted data integrity.

Also please note this project serializes and deserializes only data compatible with JSON, so that sockets, or anything else with internal classes different from those allowed by JSON standard, won't be serialized and unserialized as expected.


### New in V1: Exact same JSON API

  * Added a [reviver](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/parse#Syntax) parameter to `.parse(string, reviver)` and revive your own objects.
  * Added a [replacer](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify#Syntax) and a `space` parameter to `.stringify(object, replacer, space)` for feature parity with JSON signature.


### Compatibility
All ECMAScript engines compatible with `Map`, `Set`, `Object.keys`, and `Array.prototype.reduce` will work, even if polyfilled.


### How does it work ?
While stringifying, all Objects, including Arrays, and strings, are flattened out and replaced as unique index. `*`

Once parsed, all indexes will be replaced through the flattened collection.

<sup><sub>`*` represented as string to avoid conflicts with numbers</sub></sup>

```js
// logic example
var a = [{one: 1}, {two: '2'}];
a[0].a = a;
// a is the main object, will be at index '0'
// {one: 1} is the second object, index '1'
// {two: '2'} the third, in '2', and it has a string
// which will be found at index '3'

Flatted.stringify(a);
// [["1","2"],{"one":1,"a":"0"},{"two":"3"},"2"]
// a[one,two]    {one: 1, a}    {two: '2'}  '2'
```
