# x-is-object

<!--
    [![build status][1]][2]
    [![NPM version][3]][4]
    [![Coverage Status][5]][6]
    [![gemnasium Dependency Status][7]][8]
    [![Davis Dependency status][9]][10]
-->

<!-- [![browser support][11]][12] -->

Simple proper object test - all objects that inherit Object except null

## Example

```js
var isObject = require("x-is-object")

isObject({})
// -> true

isObject([])
// -> true

isObject(/.*/)
// -> true

isObject(new RegExp(".*"))
// -> true

isObject(function () {})
// -> true

isObject(new Date())
// -> true

isObject(new String("hello"))
// -> true

isObject("hello")
// -> false

isObject("")
// -> false

isObject(9)
// -> false

isObject(true)
// -> false

isObject(null)
// -> false

isObject(undefined)
// -> false
```

## Installation

`npm install x-is-object`

## Contributors

 - Matt-Esch

## MIT Licenced

  [1]: https://secure.travis-ci.org/Matt-Esch/x-is-object.png
  [2]: https://travis-ci.org/Matt-Esch/x-is-object
  [3]: https://badge.fury.io/js/x-is-object.png
  [4]: https://badge.fury.io/js/x-is-object
  [5]: https://coveralls.io/repos/Matt-Esch/x-is-object/badge.png
  [6]: https://coveralls.io/r/Matt-Esch/x-is-object
  [7]: https://gemnasium.com/Matt-Esch/x-is-object.png
  [8]: https://gemnasium.com/Matt-Esch/x-is-object
  [9]: https://david-dm.org/Matt-Esch/x-is-object.png
  [10]: https://david-dm.org/Matt-Esch/x-is-object
  [11]: https://ci.testling.com/Matt-Esch/x-is-object.png
  [12]: https://ci.testling.com/Matt-Esch/x-is-object
