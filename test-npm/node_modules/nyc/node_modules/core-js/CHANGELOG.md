## Changelog
##### 2.4.1 - 2016.07.18
- fixed `script` tag for some parsers, [#204](https://github.com/zloirock/core-js/issues/204), [#216](https://github.com/zloirock/core-js/issues/216)
- removed some unused variables, [#217](https://github.com/zloirock/core-js/issues/217), [#218](https://github.com/zloirock/core-js/issues/218)
- fixed MS Edge `Reflect.construct` and `Reflect.apply` - they should not allow primitive as `argumentsList` argument

##### 1.2.7 [LEGACY] - 2016.07.18
- some fixes for issues like [#159](https://github.com/zloirock/core-js/issues/159), [#186](https://github.com/zloirock/core-js/issues/186), [#194](https://github.com/zloirock/core-js/issues/194), [#207](https://github.com/zloirock/core-js/issues/207)

##### 2.4.0 - 2016.05.08
- Added `Observable`, [stage 1 proposal](https://github.com/zenparsing/es-observable)
- Fixed behavior `Object.{getOwnPropertySymbols, getOwnPropertyDescriptor}` and `Object#propertyIsEnumerable` on `Object.prototype`
- `Reflect.construct` and `Reflect.apply` should throw an error if `argumentsList` argument is not an object, [#194](https://github.com/zloirock/core-js/issues/194)

##### 2.3.0 - 2016.04.24
- Added `asap` for enqueuing microtasks, [stage 0 proposal](https://github.com/rwaldron/tc39-notes/blob/master/es6/2014-09/sept-25.md#510-globalasap-for-enqueuing-a-microtask)
- Added well-known symbol `Symbol.asyncIterator` for [stage 2 async iteration proposal](https://github.com/tc39/proposal-async-iteration)
- Added well-known symbol `Symbol.observable` for [stage 1 observables proposal](https://github.com/zenparsing/es-observable)
- `String#{padStart, padEnd}` returns original string if filler is empty string, [TC39 meeting notes](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-03/march-29.md#stringprototypepadstartpadend)
- `Object.values` and `Object.entries` moved to stage 4 from 3, [TC39 meeting notes](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-03/march-29.md#objectvalues--objectentries)
- `System.global` moved to stage 2 from 1, [TC39 meeting notes](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-03/march-29.md#systemglobal)
- `Map#toJSON` and `Set#toJSON` rejected and will be removed from the next major release, [TC39 meeting notes](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-03/march-31.md#mapprototypetojsonsetprototypetojson)
- `Error.isError` withdrawn and will be removed from the next major release, [TC39 meeting notes](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-03/march-29.md#erroriserror)
- Added fallback for `Function#name` on non-extensible functions and functions with broken `toString` conversion, [#193](https://github.com/zloirock/core-js/issues/193)

##### 2.2.2 - 2016.04.06
- Added conversion `-0` to `+0` to `Array#{indexOf, lastIndexOf}`, [ES2016 fix](https://github.com/tc39/ecma262/pull/316)
- Added fixes for some `Math` methods in Tor Browser
- `Array.{from, of}` no longer calls prototype setters
- Added workaround over Chrome DevTools strange behavior, [#186](https://github.com/zloirock/core-js/issues/186)

##### 2.2.1 - 2016.03.19
- Fixed `Object.getOwnPropertyNames(window)` `2.1+` versions bug, [#181](https://github.com/zloirock/core-js/issues/181)

##### 2.2.0 - 2016.03.15
- Added `String#matchAll`, [proposal](https://github.com/tc39/String.prototype.matchAll)
- Added `Object#__(define|lookup)[GS]etter__`, [annex B ES2017](https://github.com/tc39/ecma262/pull/381)
- Added `@@toPrimitive` methods to `Date` and `Symbol`
- Fixed `%TypedArray%#slice` in Edge ~ 13 (throws with `@@species` and wrapped / inherited constructor)
- Some other minor fixes

##### 2.1.5 - 2016.03.12
- Improved support NodeJS domains in `Promise#then`, [#180](https://github.com/zloirock/core-js/issues/180)
- Added fallback for `Date#toJSON` bug in Qt Script, [#173](https://github.com/zloirock/core-js/issues/173#issuecomment-193972502)

##### 2.1.4 - 2016.03.08
- Added fallback for `Symbol` polyfill in Qt Script, [#173](https://github.com/zloirock/core-js/issues/173)
- Added one more fallback for IE11 `Script Access Denied` error with iframes, [#165](https://github.com/zloirock/core-js/issues/165)

##### 2.1.3 - 2016.02.29
- Added fallback for [`es6-promise` package bug](https://github.com/stefanpenner/es6-promise/issues/169), [#176](https://github.com/zloirock/core-js/issues/176)

##### 2.1.2 - 2016.02.29
- Some minor `Promise` fixes:
  - Browsers `rejectionhandled` event better HTML spec complaint
  - Errors in unhandled rejection handlers should not cause any problems
  - Fixed typo in feature detection

##### 2.1.1 - 2016.02.22
- Some `Promise` improvements:
  - Feature detection:
    - **Added detection unhandled rejection tracking support - now it's available everywhere**, [#140](https://github.com/zloirock/core-js/issues/140)
    - Added detection `@@species` pattern support for completely correct subclassing
    - Removed usage `Object.setPrototypeOf` from feature detection and noisy console message about it in FF
  - `Promise.all` fixed for some very specific cases

##### 2.1.0 - 2016.02.09
- **API**:
  - ES5 polyfills are split and logic, used in other polyfills, moved to internal modules
    - **All entry point works in ES3 environment like IE8- without `core-js/(library/)es5`**
    - **Added all missed single entry points for ES5 polyfills**
    - Separated ES5 polyfills moved to the ES6 namespace. Why?
      - Mainly, for prevent duplication features in different namespaces - logic of most required ES5 polyfills changed in ES6+:
        - Already added changes for: `Object` statics - should accept primitives, new whitespaces lists in `String#trim`, `parse(Int|float)`, `RegExp#toString` logic, `String#split`, etc
        - Should be changed in the future: `@@species` and `ToLength` logic in `Array` methods, `Date` parsing, `Function#bind`, etc
        - Should not be changed only several features like `Array.isArray` and `Date.now`
      - Some ES5 polyfills required for modern engines
    - All old entry points should work fine, but in the next major release API can be changed
  - `Object.getOwnPropertyDescriptors` moved to the stage 3, [January TC39 meeting](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-01/2016-01-28.md#objectgetownpropertydescriptors-to-stage-3-jordan-harband-low-priority-but-super-quick)
  - Added `umd` option for [custom build process](https://github.com/zloirock/core-js#custom-build-from-external-scripts), [#169](https://github.com/zloirock/core-js/issues/169)
  - Returned entry points for `Array` statics, removed in `2.0`, for compatibility with `babel` `6` and for future fixes
- **Deprecated**:
  - `Reflect.enumerate` deprecated and will be removed from the next major release, [January TC39 meeting](https://github.com/rwaldron/tc39-notes/blob/master/es7/2016-01/2016-01-28.md#5xix-revisit-proxy-enumerate---revisit-decision-to-exhaust-iterator)
- **New Features**:
  - Added [`Reflect` metadata API](https://github.com/jonathandturner/decorators/blob/master/specs/metadata.md) as a pre-strawman feature, [#152](https://github.com/zloirock/core-js/issues/152):
    - `Reflect.defineMetadata`
    - `Reflect.deleteMetadata`
    - `Reflect.getMetadata`
    - `Reflect.getMetadataKeys`
    - `Reflect.getOwnMetadata`
    - `Reflect.getOwnMetadataKeys`
    - `Reflect.hasMetadata`
    - `Reflect.hasOwnMetadata`
    - `Reflect.metadata`
  - Implementation / fixes `Date#toJSON`
  - Fixes for `parseInt` and `Number.parseInt`
  - Fixes for `parseFloat` and `Number.parseFloat`
  - Fixes for `RegExp#toString`
  - Fixes for `Array#sort`
  - Fixes for `Number#toFixed`
  - Fixes for `Number#toPrecision`
  - Additional fixes for `String#split` (`RegExp#@@split`)
- **Improvements**:
  - Correct subclassing wrapped collections, `Number` and `RegExp` constructors with native class syntax
  - Correct support `SharedArrayBuffer` and buffers from other realms in typed arrays wrappers 
  - Additional validations for `Object.{defineProperty, getOwnPropertyDescriptor}` and `Reflect.defineProperty`
- **Bug Fixes**:
  - Fixed some cases `Array#lastIndexOf` with negative second argument

##### 2.0.3 - 2016.01.11
- Added fallback for V8 ~ Chrome 49 `Promise` subclassing bug causes unhandled rejection on feature detection, [#159](https://github.com/zloirock/core-js/issues/159)
- Added fix for very specific environments with global `window === null`

##### 2.0.2 - 2016.01.04
- Temporarily removed `length` validation from `Uint8Array` constructor wrapper. Reason - [bug in `ws` module](https://github.com/websockets/ws/pull/645) (-> `socket.io`) which passes to `Buffer` constructor -> `Uint8Array` float and uses [the `V8` bug](https://code.google.com/p/v8/issues/detail?id=4552) for conversion to int (by the spec should be thrown an error). [It creates problems for many people.](https://github.com/karma-runner/karma/issues/1768) I hope, it will be returned after fixing this bug in `V8`.

##### 2.0.1 - 2015.12.31
- forced usage `Promise.resolve` polyfill in the `library` version for correct work with wrapper
- `Object.assign` should be defined in the strict mode -> throw an error on extension non-extensible objects, [#154](https://github.com/zloirock/core-js/issues/154)

##### 2.0.0 - 2015.12.24
- added implementations and fixes [Typed Arrays](https://github.com/zloirock/core-js#ecmascript-6-typed-arrays)-related features
  - `ArrayBuffer`, `ArrayBuffer.isView`, `ArrayBuffer#slice`
  - `DataView` with all getter / setter methods
  - `Int8Array`, `Uint8Array`, `Uint8ClampedArray`, `Int16Array`, `Uint16Array`, `Int32Array`, `Uint32Array`, `Float32Array` and `Float64Array` constructors
  - `%TypedArray%.{for, of}`, `%TypedArray%#{copyWithin, every, fill, filter, find, findIndex, forEach, indexOf, includes, join, lastIndexOf, map, reduce, reduceRight, reverse, set, slice, some, sort, subarray, values, keys, entries, @@iterator, ...}`
- added [`System.global`](https://github.com/zloirock/core-js#ecmascript-7-proposals), [proposal](https://github.com/tc39/proposal-global), [November TC39 meeting](https://github.com/rwaldron/tc39-notes/tree/master/es7/2015-11/nov-19.md#systemglobal-jhd)
- added [`Error.isError`](https://github.com/zloirock/core-js#ecmascript-7-proposals), [proposal](https://github.com/ljharb/proposal-is-error), [November TC39 meeting](https://github.com/rwaldron/tc39-notes/tree/master/es7/2015-11/nov-19.md#jhd-erroriserror)
- added [`Math.{iaddh, isubh, imulh, umulh}`](https://github.com/zloirock/core-js#ecmascript-7-proposals), [proposal](https://gist.github.com/BrendanEich/4294d5c212a6d2254703)
- `RegExp.escape` moved from the `es7` to the non-standard `core` namespace, [July TC39 meeting](https://github.com/rwaldron/tc39-notes/blob/master/es7/2015-07/july-28.md#62-regexpescape) - too slow, but it's condition of stability, [#116](https://github.com/zloirock/core-js/issues/116)
- [`Promise`](https://github.com/zloirock/core-js#ecmascript-6-promise)
  - some performance optimisations
  - added basic support [`rejectionHandled` event / `onrejectionhandled` handler](https://github.com/zloirock/core-js#unhandled-rejection-tracking) to the polyfill
  - removed usage `@@species` from `Promise.{all, race}`, [November TC39 meeting](https://github.com/rwaldron/tc39-notes/tree/master/es7/2015-11/nov-18.md#conclusionresolution-2)
- some improvements [collections polyfills](https://github.com/zloirock/core-js#ecmascript-6-collections)
  - `O(1)` and preventing possible leaks with frozen keys, [#134](https://github.com/zloirock/core-js/issues/134)
  - correct observable state object keys
- renamed `String#{padLeft, padRight}` -> [`String#{padStart, padEnd}`](https://github.com/zloirock/core-js#ecmascript-7-proposals), [proposal](https://github.com/tc39/proposal-string-pad-start-end), [November TC39 meeting](https://github.com/rwaldron/tc39-notes/tree/master/es7/2015-11/nov-17.md#conclusionresolution-2) (they want to rename it on each meeting?O_o), [#132](https://github.com/zloirock/core-js/issues/132)
- added [`String#{trimStart, trimEnd}` as aliases for `String#{trimLeft, trimRight}`](https://github.com/zloirock/core-js#ecmascript-7-proposals), [proposal](https://github.com/sebmarkbage/ecmascript-string-left-right-trim), [November TC39 meeting](https://github.com/rwaldron/tc39-notes/tree/master/es7/2015-11/nov-17.md#conclusionresolution-2)
- added [annex B HTML methods](https://github.com/zloirock/core-js#ecmascript-6-string) - ugly, but also [the part of the spec](http://www.ecma-international.org/ecma-262/6.0/#sec-string.prototype.anchor)
- added little fix for [`Date#toString`](https://github.com/zloirock/core-js#ecmascript-6-date) - `new Date(NaN).toString()` [should be `'Invalid Date'`](http://www.ecma-international.org/ecma-262/6.0/#sec-todatestring)
- added [`{keys, values, entries, @@iterator}` methods to DOM collections](https://github.com/zloirock/core-js#iterable-dom-collections) which should have [iterable interface](https://heycam.github.io/webidl/#idl-iterable) or should be [inherited from `Array`](https://heycam.github.io/webidl/#LegacyArrayClass) - `NodeList`, `DOMTokenList`, `MediaList`, `StyleSheetList`, `CSSRuleList`.
- removed Mozilla `Array` generics - [deprecated and will be removed from FF](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array#Array_generic_methods), [looks like strawman is dead](http://wiki.ecmascript.org/doku.php?id=strawman:array_statics), available [alternative shim](https://github.com/plusdude/array-generics)
- removed `core.log` module
- CommonJS API
  - added entry points for [virtual methods](https://github.com/zloirock/core-js#commonjs-and-prototype-methods-without-global-namespace-pollution)
  - added entry points for [stages proposals](https://github.com/zloirock/core-js#ecmascript-7-proposals)
  - some other minor changes
- [custom build from external scripts](https://github.com/zloirock/core-js#custom-build-from-external-scripts) moved to the separate package for preventing problems with dependencies
- changed `$` prefix for internal modules file names because Team Foundation Server does not support it, [#129](https://github.com/zloirock/core-js/issues/129)
- additional fix for `SameValueZero` in V8 ~ Chromium 39-42 collections
- additional fix for FF27 `Array` iterator
- removed usage shortcuts for `arguments` object - old WebKit bug, [#150](https://github.com/zloirock/core-js/issues/150)
- `{Map, Set}#forEach` non-generic, [#144](https://github.com/zloirock/core-js/issues/144)
- many other improvements

##### 1.2.6 - 2015.11.09
* reject with `TypeError` on attempt resolve promise itself
* correct behavior with broken `Promise` subclass constructors / methods
* added `Promise`-based fallback for microtask
* fixed V8 and FF `Array#{values, @@iterator}.name`
* fixed IE7- `[1, 2].join(undefined) -> '1,2'`
* some other fixes / improvements / optimizations

##### 1.2.5 - 2015.11.02
* some more `Number` constructor fixes:
  * fixed V8 ~ Node 0.8 bug: `Number('+0x1')` should be `NaN`
  * fixed `Number(' 0b1\n')` case, should be `1`
  * fixed `Number()` case, should be `0`

##### 1.2.4 - 2015.11.01
* fixed `Number('0b12') -> NaN` case in the shim
* fixed V8 ~ Chromium 40- bug - `Weak(Map|Set)#{delete, get, has}` should not throw errors [#124](https://github.com/zloirock/core-js/issues/124)
* some other fixes and optimizations

##### 1.2.3 - 2015.10.23
* fixed some problems related old V8 bug `Object('a').propertyIsEnumerable(0) // => false`, for example, `Object.assign({}, 'qwe')` from the last release
* fixed `.name` property and `Function#toString` conversion some polyfilled methods
* fixed `Math.imul` arity in Safari 8-

##### 1.2.2 - 2015.10.18
* improved optimisations for V8
* fixed build process from external packages, [#120](https://github.com/zloirock/core-js/pull/120)
* one more `Object.{assign, values, entries}` fix for [**very** specific case](https://github.com/ljharb/proposal-object-values-entries/issues/5)

##### 1.2.1 - 2015.10.02
* replaced fix `JSON.stringify` + `Symbol` behavior from `.toJSON` method to wrapping `JSON.stringify` - little more correct, [compat-table/642](https://github.com/kangax/compat-table/pull/642)
* fixed typo which broke tasks scheduler in WebWorkers in old FF, [#114](https://github.com/zloirock/core-js/pull/114)

##### 1.2.0 - 2015.09.27
* added browser [`Promise` rejection hook](#unhandled-rejection-tracking), [#106](https://github.com/zloirock/core-js/issues/106)
* added correct [`IsRegExp`](http://www.ecma-international.org/ecma-262/6.0/#sec-isregexp) logic to [`String#{includes, startsWith, endsWith}`](https://github.com/zloirock/core-js/#ecmascript-6-string) and [`RegExp` constructor](https://github.com/zloirock/core-js/#ecmascript-6-regexp), `@@match` case, [example](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Symbol/match#Disabling_the_isRegExp_check)
* updated [`String#leftPad`](https://github.com/zloirock/core-js/#ecmascript-7-proposals) [with proposal](https://github.com/ljharb/proposal-string-pad-left-right/issues/6): string filler truncated from the right side
* replaced V8 [`Object.assign`](https://github.com/zloirock/core-js/#ecmascript-6-object) - its properties order not only [incorrect](https://github.com/sindresorhus/object-assign/issues/22), it is non-deterministic and it causes some problems
* fixed behavior with deleted in getters properties for `Object.{`[`assign`](https://github.com/zloirock/core-js/#ecmascript-6-object)`, `[`entries, values`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)`}`, [example](http://goo.gl/iQE01c)
* fixed [`Math.sinh`](https://github.com/zloirock/core-js/#ecmascript-6-math) with very small numbers in V8 near Chromium 38
* some other fixes and optimizations

##### 1.1.4 - 2015.09.05
* fixed support symbols in FF34-35 [`Object.assign`](https://github.com/zloirock/core-js/#ecmascript-6-object)
* fixed [collections iterators](https://github.com/zloirock/core-js/#ecmascript-6-iterators) in FF25-26
* fixed non-generic WebKit [`Array.of`](https://github.com/zloirock/core-js/#ecmascript-6-array)
* some other fixes and optimizations

##### 1.1.3 - 2015.08.29
* fixed support Node.js domains in [`Promise`](https://github.com/zloirock/core-js/#ecmascript-6-promise), [#103](https://github.com/zloirock/core-js/issues/103)

##### 1.1.2 - 2015.08.28
* added `toJSON` method to [`Symbol`](https://github.com/zloirock/core-js/#ecmascript-6-symbol) polyfill and to MS Edge implementation for expected `JSON.stringify` result w/o patching this method
* replaced [`Reflect.construct`](https://github.com/zloirock/core-js/#ecmascript-6-reflect) implementations w/o correct support third argument
* fixed `global` detection with changed `document.domain` in ~IE8, [#100](https://github.com/zloirock/core-js/issues/100)

##### 1.1.1 - 2015.08.20
* added more correct microtask implementation for [`Promise`](#ecmascript-6-promise)

##### 1.1.0 - 2015.08.17
* updated [string padding](https://github.com/zloirock/core-js/#ecmascript-7-proposals) to [actual proposal](https://github.com/ljharb/proposal-string-pad-left-right) - renamed, minor internal changes:
  * `String#lpad` -> `String#padLeft`
  * `String#rpad` -> `String#padRight`
* added [string trim functions](#ecmascript-7-proposals) - [proposal](https://github.com/sebmarkbage/ecmascript-string-left-right-trim), defacto standard - required only for IE11- and fixed for some old engines:
  * `String#trimLeft`
  * `String#trimRight`
* [`String#trim`](https://github.com/zloirock/core-js/#ecmascript-6-string) fixed for some engines by es6 spec and moved from `es5` to single `es6` module
* splitted [`es6.object.statics-accept-primitives`](https://github.com/zloirock/core-js/#ecmascript-6-object)
* caps for `freeze`-family `Object` methods moved from `es5` to `es6` namespace and joined with [es6 wrappers](https://github.com/zloirock/core-js/#ecmascript-6-object)
* `es5` [namespace](https://github.com/zloirock/core-js/#commonjs) also includes modules, moved to `es6` namespace - you can use it as before
* increased `MessageChannel` priority in `$.task`, [#95](https://github.com/zloirock/core-js/issues/95)
* does not get `global.Symbol` on each getting iterator, if you wanna use alternative `Symbol` shim - add it before `core-js`
* [`Reflect.construct`](https://github.com/zloirock/core-js/#ecmascript-6-reflect) optimized and fixed for some cases
* simplified [`Reflect.enumerate`](https://github.com/zloirock/core-js/#ecmascript-6-reflect), see [this question](https://esdiscuss.org/topic/question-about-enumerate-and-property-decision-timing)
* some corrections in [`Math.acosh`](https://github.com/zloirock/core-js/#ecmascript-6-math)
* fixed [`Math.imul`](https://github.com/zloirock/core-js/#ecmascript-6-math) for old WebKit
* some fixes in string / RegExp [well-known symbols](https://github.com/zloirock/core-js/#ecmascript-6-regexp) logic
* some other fixes and optimizations

##### 1.0.1 - 2015.07.31
* some fixes for final MS Edge, replaced broken native `Reflect.defineProperty`
* some minor fixes and optimizations
* changed compression `client/*.min.js` options for safe `Function#name` and `Function#length`, should be fixed [#92](https://github.com/zloirock/core-js/issues/92)

##### 1.0.0 - 2015.07.22
* added logic for [well-known symbols](https://github.com/zloirock/core-js/#ecmascript-6-regexp):
  * `Symbol.match`
  * `Symbol.replace`
  * `Symbol.split`
  * `Symbol.search`
* actualized and optimized work with iterables:
  * optimized  [`Map`, `Set`, `WeakMap`, `WeakSet` constructors](https://github.com/zloirock/core-js/#ecmascript-6-collections), [`Promise.all`, `Promise.race`](https://github.com/zloirock/core-js/#ecmascript-6-promise) for default `Array Iterator`
  * optimized  [`Array.from`](https://github.com/zloirock/core-js/#ecmascript-6-array) for default `Array Iterator`
  * added [`core.getIteratorMethod`](https://github.com/zloirock/core-js/#ecmascript-6-iterators) helper
* uses enumerable properties in shimmed instances - collections, iterators, etc for optimize performance
* added support native constructors to [`Reflect.construct`](https://github.com/zloirock/core-js/#ecmascript-6-reflect) with 2 arguments
* added support native constructors to [`Function#bind`](https://github.com/zloirock/core-js/#ecmascript-5) shim with `new`
* removed obsolete `.clear` methods native [`Weak`-collections](https://github.com/zloirock/core-js/#ecmascript-6-collections)
* maximum modularity, reduced minimal custom build size, separated into submodules:
  * [`es6.reflect`](https://github.com/zloirock/core-js/#ecmascript-6-reflect)
  * [`es6.regexp`](https://github.com/zloirock/core-js/#ecmascript-6-regexp)
  * [`es6.math`](https://github.com/zloirock/core-js/#ecmascript-6-math)
  * [`es6.number`](https://github.com/zloirock/core-js/#ecmascript-6-number)
  * [`es7.object.to-array`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)
  * [`core.object`](https://github.com/zloirock/core-js/#object)
  * [`core.string`](https://github.com/zloirock/core-js/#escaping-strings)
  * [`core.iter-helpers`](https://github.com/zloirock/core-js/#ecmascript-6-iterators)
  * internal modules (`$`, `$.iter`, etc)
* many other optimizations
* final cleaning non-standard features
  * moved `$for` to [separate library](https://github.com/zloirock/forof). This work for syntax - `for-of` loop and comprehensions
  * moved `Date#{format, formatUTC}` to [separate library](https://github.com/zloirock/dtf). Standard way for this - `ECMA-402`
  * removed `Math` methods from `Number.prototype`. Slight sugar for simple `Math` methods calling
  * removed `{Array#, Array, Dict}.turn`
  * removed `core.global`
* uses `ToNumber` instead of `ToLength` in [`Number Iterator`](https://github.com/zloirock/core-js/#number-iterator), `Array.from(2.5)` will be `[0, 1, 2]` instead of `[0, 1]`
* fixed [#85](https://github.com/zloirock/core-js/issues/85) - invalid `Promise` unhandled rejection message in nested `setTimeout`
* fixed [#86](https://github.com/zloirock/core-js/issues/86) - support FF extensions
* fixed [#89](https://github.com/zloirock/core-js/issues/89) - behavior `Number` constructor in strange case

##### 0.9.18 - 2015.06.17
* removed `/` from [`RegExp.escape`](https://github.com/zloirock/core-js/#ecmascript-7-proposals) escaped characters

##### 0.9.17 - 2015.06.14
* updated [`RegExp.escape`](https://github.com/zloirock/core-js/#ecmascript-7-proposals) to the [latest proposal](https://github.com/benjamingr/RexExp.escape)
* fixed conflict with webpack dev server + IE buggy behavior

##### 0.9.16 - 2015.06.11
* more correct order resolving thenable in [`Promise`](https://github.com/zloirock/core-js/#ecmascript-6-promise) polyfill
* uses polyfill instead of [buggy V8 `Promise`](https://github.com/zloirock/core-js/issues/78)

##### 0.9.15 - 2015.06.09
* [collections](https://github.com/zloirock/core-js/#ecmascript-6-collections) from `library` version return wrapped native instances
* fixed collections prototype methods in `library` version
* optimized [`Math.hypot`](https://github.com/zloirock/core-js/#ecmascript-6-math)

##### 0.9.14 - 2015.06.04
* updated [`Promise.resolve` behavior](https://esdiscuss.org/topic/fixing-promise-resolve)
* added fallback for IE11 buggy `Object.getOwnPropertyNames` + iframe
* some other fixes

##### 0.9.13 - 2015.05.25
* added fallback for [`Symbol` polyfill](https://github.com/zloirock/core-js/#ecmascript-6-symbol) for old Android
* some other fixes

##### 0.9.12 - 2015.05.24
* different instances `core-js` should use / recognize the same symbols
* some fixes

##### 0.9.11 - 2015.05.18
* simplified [custom build](https://github.com/zloirock/core-js/#custom-build)
  * add custom build js api
  * added `grunt-cli` to `devDependencies` for `npm run grunt`
* some fixes

##### 0.9.10 - 2015.05.16
* wrapped `Function#toString` for correct work wrapped methods / constructors with methods similar to the [`lodash` `isNative`](https://github.com/lodash/lodash/issues/1197)
* added proto versions of methods to export object in `default` version for consistency with `library` version

##### 0.9.9 - 2015.05.14
* wrapped `Object#propertyIsEnumerable` for [`Symbol` polyfill](https://github.com/zloirock/core-js/#ecmascript-6-symbol)
* [added proto versions of methods to `library` for ES7 bind syntax](https://github.com/zloirock/core-js/issues/65)
* some other fixes

##### 0.9.8 - 2015.05.12
* fixed [`Math.hypot`](https://github.com/zloirock/core-js/#ecmascript-6-math) with negative arguments
* added `Object#toString.toString` as fallback for [`lodash` `isNative`](https://github.com/lodash/lodash/issues/1197)

##### 0.9.7 - 2015.05.07
* added [support DOM collections](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/slice#Streamlining_cross-browser_behavior) to IE8- `Array#slice`

##### 0.9.6 - 2015.05.01
* added [`String#lpad`, `String#rpad`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)

##### 0.9.5 - 2015.04.30
* added cap for `Function#@@hasInstance`
* some fixes and optimizations

##### 0.9.4 - 2015.04.27
* fixed `RegExp` constructor

##### 0.9.3 - 2015.04.26
* some fixes and optimizations

##### 0.9.2 - 2015.04.25
* more correct [`Promise`](https://github.com/zloirock/core-js/#ecmascript-6-promise) unhandled rejection tracking and resolving / rejection priority

##### 0.9.1 - 2015.04.25
* fixed `__proto__`-based [`Promise`](https://github.com/zloirock/core-js/#ecmascript-6-promise) subclassing in some environments

##### 0.9.0 - 2015.04.24
* added correct [symbols](https://github.com/zloirock/core-js/#ecmascript-6-symbol) descriptors
  * fixed behavior `Object.{assign, create, defineProperty, defineProperties, getOwnPropertyDescriptor, getOwnPropertyDescriptors}` with symbols
  * added [single entry points](https://github.com/zloirock/core-js/#commonjs) for `Object.{create, defineProperty, defineProperties}`
* added [`Map#toJSON`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)
* removed non-standard methods `Object#[_]` and `Function#only` - they solves syntax problems, but now in compilers available arrows and ~~in near future will be available~~ [available](http://babeljs.io/blog/2015/05/14/function-bind/) [bind syntax](https://github.com/zenparsing/es-function-bind)
* removed non-standard undocumented methods `Symbol.{pure, set}`
* some fixes and internal changes

##### 0.8.4 - 2015.04.18
* uses `webpack` instead of `browserify` for browser builds - more compression-friendly result

##### 0.8.3 - 2015.04.14
* fixed `Array` statics with single entry points

##### 0.8.2 - 2015.04.13
* [`Math.fround`](https://github.com/zloirock/core-js/#ecmascript-6-math) now also works in IE9-
* added [`Set#toJSON`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)
* some optimizations and fixes

##### 0.8.1 - 2015.04.03
* fixed `Symbol.keyFor`

##### 0.8.0 - 2015.04.02
* changed [CommonJS API](https://github.com/zloirock/core-js/#commonjs)
* splitted and renamed some modules
* added support ES3 environment (ES5 polyfill) to **all** default versions - size increases slightly (+ ~4kb w/o gzip), many issues disappear, if you don't need it - [simply include only required namespaces / features / modules](https://github.com/zloirock/core-js/#commonjs)
* removed [abstract references](https://github.com/zenparsing/es-abstract-refs) support - proposal has been superseded =\
* [`$for.isIterable` -> `core.isIterable`, `$for.getIterator` -> `core.getIterator`](https://github.com/zloirock/core-js/#ecmascript-6-iterators), temporary available in old namespace
* fixed iterators support in v8 `Promise.all` and `Promise.race`
* many other fixes

##### 0.7.2 - 2015.03.09
* some fixes

##### 0.7.1 - 2015.03.07
* some fixes

##### 0.7.0 - 2015.03.06
* rewritten and splitted into [CommonJS modules](https://github.com/zloirock/core-js/#commonjs)

##### 0.6.1 - 2015.02.24
* fixed support [`Object.defineProperty`](https://github.com/zloirock/core-js/#ecmascript-5) with accessors on DOM elements on IE8

##### 0.6.0 - 2015.02.23
* added support safe closing iteration - calling `iterator.return` on abort iteration, if it exists
* added basic support [`Promise`](https://github.com/zloirock/core-js/#ecmascript-6-promise) unhandled rejection tracking in shim
* added [`Object.getOwnPropertyDescriptors`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)
* removed `console` cap - creates too many problems
* restructuring [namespaces](https://github.com/zloirock/core-js/#custom-build)
* some fixes

##### 0.5.4 - 2015.02.15
* some fixes

##### 0.5.3 - 2015.02.14
* added [support binary and octal literals](https://github.com/zloirock/core-js/#ecmascript-6-number) to `Number` constructor
* added [`Date#toISOString`](https://github.com/zloirock/core-js/#ecmascript-5)

##### 0.5.2 - 2015.02.10
* some fixes

##### 0.5.1 - 2015.02.09
* some fixes

##### 0.5.0 - 2015.02.08
* systematization of modules
* splitted [`es6` module](https://github.com/zloirock/core-js/#ecmascript-6)
* splitted `console` module: `web.console` - only cap for missing methods, `core.log` - bound methods & additional features
* added [`delay` method](https://github.com/zloirock/core-js/#delay)
* some fixes

##### 0.4.10 - 2015.01.28
* [`Object.getOwnPropertySymbols`](https://github.com/zloirock/core-js/#ecmascript-6-symbol) polyfill returns array of wrapped keys

##### 0.4.9 - 2015.01.27
* FF20-24 fix

##### 0.4.8 - 2015.01.25
* some [collections](https://github.com/zloirock/core-js/#ecmascript-6-collections) fixes

##### 0.4.7 - 2015.01.25
* added support frozen objects as [collections](https://github.com/zloirock/core-js/#ecmascript-6-collections) keys

##### 0.4.6 - 2015.01.21
* added [`Object.getOwnPropertySymbols`](https://github.com/zloirock/core-js/#ecmascript-6-symbol)
* added [`NodeList.prototype[@@iterator]`](https://github.com/zloirock/core-js/#ecmascript-6-iterators)
* added basic `@@species` logic - getter in native constructors
* removed `Function#by`
* some fixes

##### 0.4.5 - 2015.01.16
* some fixes

##### 0.4.4 - 2015.01.11
* enabled CSP support

##### 0.4.3 - 2015.01.10
* added `Function` instances `name` property for IE9+

##### 0.4.2 - 2015.01.10
* `Object` static methods accept primitives
* `RegExp` constructor can alter flags (IE9+)
* added `Array.prototype[Symbol.unscopables]`

##### 0.4.1 - 2015.01.05
* some fixes

##### 0.4.0 - 2015.01.03
* added [`es6.reflect`](https://github.com/zloirock/core-js/#ecmascript-6-reflect) module:
  * added `Reflect.apply`
  * added `Reflect.construct`
  * added `Reflect.defineProperty`
  * added `Reflect.deleteProperty`
  * added `Reflect.enumerate`
  * added `Reflect.get`
  * added `Reflect.getOwnPropertyDescriptor`
  * added `Reflect.getPrototypeOf`
  * added `Reflect.has`
  * added `Reflect.isExtensible`
  * added `Reflect.preventExtensions`
  * added `Reflect.set`
  * added `Reflect.setPrototypeOf`
* `core-js` methods now can use external `Symbol.iterator` polyfill
* some fixes

##### 0.3.3 - 2014.12.28
* [console cap](https://github.com/zloirock/core-js/#console) excluded from node.js default builds

##### 0.3.2 - 2014.12.25
* added cap for [ES5](https://github.com/zloirock/core-js/#ecmascript-5) freeze-family methods
* fixed `console` bug

##### 0.3.1 - 2014.12.23
* some fixes

##### 0.3.0 - 2014.12.23
* Optimize [`Map` & `Set`](https://github.com/zloirock/core-js/#ecmascript-6-collections):
  * use entries chain on hash table
  * fast & correct iteration
  * iterators moved to [`es6`](https://github.com/zloirock/core-js/#ecmascript-6) and [`es6.collections`](https://github.com/zloirock/core-js/#ecmascript-6-collections) modules

##### 0.2.5 - 2014.12.20
* `console` no longer shortcut for `console.log` (compatibility problems)
* some fixes

##### 0.2.4 - 2014.12.17
* better compliance of ES6
* added [`Math.fround`](https://github.com/zloirock/core-js/#ecmascript-6-math) (IE10+)
* some fixes

##### 0.2.3 - 2014.12.15
* [Symbols](https://github.com/zloirock/core-js/#ecmascript-6-symbol):
  * added option to disable addition setter to `Object.prototype` for Symbol polyfill:
    * added `Symbol.useSimple`
    * added `Symbol.useSetter`
  * added cap for well-known Symbols:
    * added `Symbol.hasInstance`
    * added `Symbol.isConcatSpreadable`
    * added `Symbol.match`
    * added `Symbol.replace`
    * added `Symbol.search`
    * added `Symbol.species`
    * added `Symbol.split`
    * added `Symbol.toPrimitive`
    * added `Symbol.unscopables`

##### 0.2.2 - 2014.12.13
* added [`RegExp#flags`](https://github.com/zloirock/core-js/#ecmascript-6-regexp) ([December 2014 Draft Rev 29](http://wiki.ecmascript.org/doku.php?id=harmony:specification_drafts#december_6_2014_draft_rev_29))
* added [`String.raw`](https://github.com/zloirock/core-js/#ecmascript-6-string)

##### 0.2.1 - 2014.12.12
* repair converting -0 to +0 in [native collections](https://github.com/zloirock/core-js/#ecmascript-6-collections)

##### 0.2.0 - 2014.12.06
* added [`es7.proposals`](https://github.com/zloirock/core-js/#ecmascript-7-proposals) and [`es7.abstract-refs`](https://github.com/zenparsing/es-abstract-refs) modules
* added [`String#at`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)
* added real [`String Iterator`](https://github.com/zloirock/core-js/#ecmascript-6-iterators), older versions used Array Iterator
* added abstract references support:
  * added `Symbol.referenceGet`
  * added `Symbol.referenceSet`
  * added `Symbol.referenceDelete`
  * added `Function#@@referenceGet`
  * added `Map#@@referenceGet`
  * added `Map#@@referenceSet`
  * added `Map#@@referenceDelete`
  * added `WeakMap#@@referenceGet`
  * added `WeakMap#@@referenceSet`
  * added `WeakMap#@@referenceDelete`
  * added `Dict.{...methods}[@@referenceGet]`
* removed deprecated `.contains` methods
* some fixes

##### 0.1.5 - 2014.12.01
* added [`Array#copyWithin`](https://github.com/zloirock/core-js/#ecmascript-6-array)
* added [`String#codePointAt`](https://github.com/zloirock/core-js/#ecmascript-6-string)
* added [`String.fromCodePoint`](https://github.com/zloirock/core-js/#ecmascript-6-string)

##### 0.1.4 - 2014.11.27
* added [`Dict.mapPairs`](https://github.com/zloirock/core-js/#dict)

##### 0.1.3 - 2014.11.20
* [TC39 November meeting](https://github.com/rwaldron/tc39-notes/tree/master/es6/2014-11):
  * [`.contains` -> `.includes`](https://github.com/rwaldron/tc39-notes/blob/master/es6/2014-11/nov-18.md#51--44-arrayprototypecontains-and-stringprototypecontains)
    * `String#contains` -> [`String#includes`](https://github.com/zloirock/core-js/#ecmascript-6-string)
    * `Array#contains` -> [`Array#includes`](https://github.com/zloirock/core-js/#ecmascript-7-proposals)
    * `Dict.contains` -> [`Dict.includes`](https://github.com/zloirock/core-js/#dict)
  * [removed `WeakMap#clear`](https://github.com/rwaldron/tc39-notes/blob/master/es6/2014-11/nov-19.md#412-should-weakmapweakset-have-a-clear-method-markm)
  * [removed `WeakSet#clear`](https://github.com/rwaldron/tc39-notes/blob/master/es6/2014-11/nov-19.md#412-should-weakmapweakset-have-a-clear-method-markm)

##### 0.1.2 - 2014.11.19
* `Map` & `Set` bug fix

##### 0.1.1 - 2014.11.18
* public release