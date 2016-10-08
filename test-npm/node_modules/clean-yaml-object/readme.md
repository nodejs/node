# clean-yaml-object [![Build Status](https://travis-ci.org/tapjs/clean-yaml-object.svg?branch=master)](https://travis-ci.org/tapjs/clean-yaml-object) [![Coverage Status](https://coveralls.io/repos/tapjs/clean-yaml-object/badge.svg?branch=master&service=github)](https://coveralls.io/github/tapjs/clean-yaml-object?branch=master)

> Clean up an object prior to serialization.

Replaces circular references, pretty prints Buffers, and numerous other enhancements. Primarily designed to prepare Errors for serialization to JSON/YAML.

Extracted from [`node-tap`](https://github.com/tapjs/node-tap)

## Install

```
$ npm install --save clean-yaml-object
```


## Usage

```js
const cleanYamlObject = require('clean-yaml-object');

cleanYamlObject(new Error('foo'));
//=> {name: 'Error', message: 'foo', stack: ...}
```


## API

### cleanYamlObject(input, [filterFn])

Returns a deep copy of `input` that is suitable for serialization. 

#### input

Type: `*`

Any object.

#### filterFn

Type: `callback(propertyName, isRoot, source, target)`

Optional filter callback. Returning `true` will cause the property to be copied. Otherwise it will be skipped

- `propertyName`: The property being copied.
- `isRoot`: `true` only if `source` is the top level object passed to `copyYamlObject`
- `source`: The source from which `source[propertyName]` will be copied.
- `target`: The target object.

## License


MIT © [Isaac Z. Schlueter](http://github.com/isaacs) [James Talmage](http://github.com/jamestalmage)
