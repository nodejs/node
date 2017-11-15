# x-is-array

Simple array test

## Example

```js
var isArray = require("x-is-array")

isArray([])
// -> true

isArray("hello")
// -> false

isArray("")
// -> false

isArray(9)
// -> false

isArray(true)
// -> false

isArray(new Date())
// -> false

isArray({
// -> false

isArray(null)
// -> false

isArray(undefined)
// -> false
```

## Installation

`npm install x-is-array`

## Contributors

 - Matt-Esch

## MIT Licenced