# find-root
recursively find the closest package.json

[![Circle CI](https://circleci.com/gh/jden/find-root.svg?style=svg)](https://circleci.com/gh/jden/find-root)

## usage
Say you want to check if the directory name of a project matches its
module name in package.json:

```js
var path = require('path')
var findRoot = require('find-root')

// from a starting directory, recursively search for the nearest
// directory containing package.json
var root = findRoot('/Users/jden/dev/find-root/tests')
// => '/Users/jden/dev/find-root'

var dirname = path.basename(root)
console.log('is it the same?')
console.log(dirname === require(path.join(root, 'package.json')).name)
```


## api

### `findRoot: (startingPath : String) => String`

Returns the path for the nearest directory to `startingPath` containing
a `package.json` file, eg `/foo/module`.

Throws an error if no `package.json` is found at any level in the
`startingPath`.


## installation

    $ npm install find-root


## running the tests

From package root:

    $ npm install
    $ npm test


## contributors

- jden <jason@denizac.org>


## license
MIT. (c) MMXIII AgileMD http://agilemd.com
