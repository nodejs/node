# ajv-keywords

Custom JSON-Schema keywords for [ajv](https://github.com/epoberezkin/ajv) validator

[![Build Status](https://travis-ci.org/epoberezkin/ajv-keywords.svg?branch=master)](https://travis-ci.org/epoberezkin/ajv-keywords)
[![npm version](https://badge.fury.io/js/ajv-keywords.svg)](https://www.npmjs.com/package/ajv-keywords)
[![Coverage Status](https://coveralls.io/repos/github/epoberezkin/ajv-keywords/badge.svg?branch=master)](https://coveralls.io/github/epoberezkin/ajv-keywords?branch=master)


## Install

```
npm install ajv-keywords
```


## Usage

To add all available keywords:

```javascript
var Ajv = require('ajv');
var ajv = new Ajv;
require('ajv-keywords')(ajv);

ajv.validate({ instanceof: 'RegExp' }, /.*/); // true
ajv.validate({ instanceof: 'RegExp' }, '.*'); // false
```

To add a single keyword:

```javascript
require('ajv-keywords')(ajv, 'instanceof');
```

To add multiple keywords:

```javascript
require('ajv-keywords')(ajv, ['typeof', 'instanceof']);
```

To add a single keyword in browser (to avoid adding unused code):

```javascript
require('ajv-keywords/keywords/instanceof')(ajv);
```


## Keywords

### `typeof`

Based on JavaScript `typeof` operation.

The value of the keyword should be a string (`"undefined"`, `"string"`, `"number"`, `"object"`, `"function"`, `"boolean"` or `"symbol"`) or array of strings.

To pass validation the result of `typeof` operation on the value should be equal to the string (or one of the strings in the array).

```
ajv.validate({ typeof: 'undefined' }, undefined); // true
ajv.validate({ typeof: 'undefined' }, null); // false
ajv.validate({ typeof: ['undefined', 'object'] }, null); // true
```


### `instanceof`

Based on JavaScript `typeof` operation.

The value of the keyword should be a string (`"Object"`, `"Array"`, `"Function"`, `"Number"`, `"String"`, `"Date"`, `"RegExp"` or `"Buffer"`) or array of strings.

To pass validation the result of `data instanceof ...` operation on the value should be true:

```
ajv.validate({ instanceof: 'Array' }, []); // true
ajv.validate({ instanceof: 'Array' }, {}); // false
ajv.validate({ instanceof: ['Array', 'Function'] }, funciton(){}); // true
```

You can add your own constructor function to be recognised by this keyword:

```javascript
function MyClass() {}
var instanceofDefinition = require('ajv-keywords').get('instanceof').definition;
// or require('ajv-keywords/keywords/instanceof').definition;
instanceofDefinition.CONSTRUCTORS.MyClass = MyClass;

ajv.validate({ instanceof: 'MyClass' }, new MyClass); // true
```


### `range` and `exclusiveRange`

Syntax sugar for the combination of minimum and maximum keywords, also fails schema compilation if there are no numbers in the range.

The value of this keyword must be the array consisting of two numbers, the second must be greater or equal than the first one.

If the validated value is not a number the validation passes, otherwise to pas validation the value should be greater (or equal) than the first number and smaller (or equal) than the second number in the array. If `exclusiveRange` keyword is present in the same schema and its value is true, the validated value must not be equal to the range boundaries.

```javascript
var schema = { range: [1, 3] };
ajv.validate(schema, 1); // true
ajv.validate(schema, 2); // true
ajv.validate(schema, 3); // true
ajv.validate(schema, 0.99); // false
ajv.validate(schema, 3.01); // false

var schema = { range: [1, 3], exclusiveRange: true };
ajv.validate(schema, 1.01); // true
ajv.validate(schema, 2); // true
ajv.validate(schema, 2.99); // true
ajv.validate(schema, 1); // false
ajv.validate(schema, 3); // false
```


### `propertyNames`

This keyword allows to define the schema for the property names of the object. The value of this keyword should be a valid JSON schema (v5 schemas are supported with Ajv option `{v5: true}`).

```javascript
var schema = {
  type: 'object'
  propertyNames: {
    anyOf: [
      { format: ipv4 },
      { format: hostname }
    ]
  }
};

var validData = {
  '192.128.0.1': {},
  'test.example.com': {}
};

var invalidData = {
  '1.2.3': {}
};

ajv.validate(schema, validData); // true
ajv.validate(schema, invalidData); // false
```


### `regexp`

This keyword allows to use regular expressions with flags in schemas (the standard `pattern` keyword does not support flags). The value of this keyword can be either a string (the result of `regexp.toString()`) or an object with the properties `pattern` and `flags` (the same strings that should be passed to RegExp constructor).

```javascript
var schema = {
  type: 'object',
  properties: {
    foo: { regexp: '/foo/i' },
    bar: { regexp: { pattern: 'bar', flags: 'i' } }
  }
};

var validData = {
  foo: 'Food',
  bar: 'Barmen'
};

var invalidData = {
  foo: 'fog',
  bar: 'bad'
};
```


## License

[MIT](https://github.com/JSONScript/ajv-keywords/blob/master/LICENSE)
