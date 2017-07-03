# x-is-function
x is a function

# usage
`npm install x-is-function`

```js
var isFunction = require('x-is-function')

isFunction(function () {})
// -> true

isFunction("hello")
// -> false

isFunction("")
// -> false

isFunction(9)
// -> false

isFunction(true)
// -> false

isFunction(new Date())
// -> false

isFunction({})
// -> false

isFunction(null)
// -> false

isFunction(undefined)
// -> false
```


# related
a list of other `x-is-...` modules can be found at
* [x-is](https://www.npmjs.com/package/x-is)
