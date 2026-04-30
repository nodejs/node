# Validator

<!--introduced_in=vREPLACEME-->

<!-- YAML
added: REPLACEME
-->

> Stability: 1.0 - Early development

> This feature is experimental and may change at any time. To disable it,
> start Node.js with [`--no-experimental-validator`][].

<!-- source_link=lib/validator.js -->

The `node:validator` module provides a schema-based object validator for
simple REST API input validation. It supports basic type checking, property
constraints, required fields, and nested schemas.

To access it:

```mjs
import { Schema } from 'node:validator';
```

```cjs
const { Schema } = require('node:validator');
```

This module is only available under the `node:` scheme.

The following example shows the basic usage of the `node:validator` module
to validate a user object.

```mjs
import { Schema } from 'node:validator';

const userSchema = new Schema({
  type: 'object',
  required: ['name', 'email'],
  properties: {
    name: { type: 'string', minLength: 1, maxLength: 100 },
    email: { type: 'string', pattern: '^[^@]+@[^@]+$' },
    age: { type: 'integer', minimum: 0, maximum: 150 },
    tags: { type: 'array', items: { type: 'string' }, maxItems: 10 },
  },
});

const result = userSchema.validate({
  name: 'Alice',
  email: 'alice@example.com',
  age: 30,
  tags: ['admin'],
});

console.log(result.valid); // true
console.log(result.errors); // []
```

```cjs
'use strict';
const { Schema } = require('node:validator');

const userSchema = new Schema({
  type: 'object',
  required: ['name', 'email'],
  properties: {
    name: { type: 'string', minLength: 1, maxLength: 100 },
    email: { type: 'string', pattern: '^[^@]+@[^@]+$' },
    age: { type: 'integer', minimum: 0, maximum: 150 },
    tags: { type: 'array', items: { type: 'string' }, maxItems: 10 },
  },
});

const result = userSchema.validate({
  name: 'Alice',
  email: 'alice@example.com',
  age: 30,
  tags: ['admin'],
});

console.log(result.valid); // true
console.log(result.errors); // []
```

## Validation error codes

<!-- YAML
added: REPLACEME
-->

The following error codes are returned in the `code` field of validation
error objects. They are also available as properties of the `codes` export.

| Code                  | Description                                                                             |
| --------------------- | --------------------------------------------------------------------------------------- |
| `INVALID_TYPE`        | Value type does not match the declared type                                             |
| `MISSING_REQUIRED`    | A required property is missing                                                          |
| `STRING_TOO_SHORT`    | String is shorter than `minLength`                                                      |
| `STRING_TOO_LONG`     | String is longer than `maxLength`                                                       |
| `PATTERN_MISMATCH`    | String does not match the `pattern` regex                                               |
| `ENUM_MISMATCH`       | Value is not one of the allowed `enum` values                                           |
| `NUMBER_TOO_SMALL`    | Number is below `minimum` or `exclusiveMinimum`                                         |
| `NUMBER_TOO_LARGE`    | Number is above `maximum` or `exclusiveMaximum`                                         |
| `NUMBER_NOT_MULTIPLE` | Number is not a multiple of `multipleOf`                                                |
| `NOT_INTEGER`         | Value is not an integer when `type` is `'integer'`                                      |
| `ARRAY_TOO_SHORT`     | Array has fewer items than `minItems`                                                   |
| `ARRAY_TOO_LONG`      | Array has more items than `maxItems`                                                    |
| `ADDITIONAL_PROPERTY` | Object has a property not listed in `properties` when `additionalProperties` is `false` |

Compare the `code` field of a validation error against the `codes` export
instead of hard-coding string literals:

```cjs
const { Schema, codes } = require('node:validator');

const schema = new Schema({ type: 'number', minimum: 0 });
const result = schema.validate(-1);
if (!result.valid && result.errors[0].code === codes.NUMBER_TOO_SMALL) {
  // Handle the below-minimum case.
}
```

## Class: `Schema`

<!-- YAML
added: REPLACEME
-->

### `new Schema(definition)`

<!-- YAML
added: REPLACEME
-->

* `definition` {Object} A schema definition object.

Creates a new `Schema` instance. The schema definition is validated and
compiled at construction time. [`ERR_VALIDATOR_INVALID_SCHEMA`][] is thrown
if the definition is invalid.

The `definition` object must have a `type` property set to one of the
supported types: `'string'`, `'number'`, `'integer'`, `'boolean'`,
`'object'`, `'array'`, or `'null'`.

#### Schema definition properties

The following properties are supported depending on the schema type.

##### All types

* `type` {string} **Required.** One of `'string'`, `'number'`, `'integer'`,
  `'boolean'`, `'object'`, `'array'`, `'null'`.
* `default` {any} Default value to apply when using
  [`schema.applyDefaults()`][].

##### Type: `'string'`

* `minLength` {number} Minimum string length (inclusive). Must be a
  non-negative integer.
* `maxLength` {number} Maximum string length (inclusive). Must be a
  non-negative integer.
* `pattern` {string} A regular expression pattern the string must match.
* `enum` {Array} An array of allowed values.

##### Type: `'number'`

* `minimum` {number} Minimum value (inclusive).
* `maximum` {number} Maximum value (inclusive).
* `exclusiveMinimum` {number} Minimum value (exclusive).
* `exclusiveMaximum` {number} Maximum value (exclusive).
* `multipleOf` {number} The value must be a multiple of this number. Must be
  greater than 0.

##### Type: `'integer'`

Same constraints as `'number'`. Additionally, the value must be an integer.

##### Type: `'array'`

* `items` {Object} A schema definition for array elements.
* `minItems` {number} Minimum array length (inclusive). Must be a
  non-negative integer.
* `maxItems` {number} Maximum array length (inclusive). Must be a
  non-negative integer.

##### Type: `'object'`

* `properties` {Object} A map of property names to schema definitions.
* `required` {string\[]} An array of required property names. Each name must
  be defined in `properties`.
* `additionalProperties` {boolean} Whether properties not listed in
  `properties` are allowed. **Default:** `true`.

### `schema.validate(data)`

<!-- YAML
added: REPLACEME
-->

* `data` {any} The value to validate.
* Returns: {ValidationResult}

Validates the given data against the schema. Returns a frozen object with
`valid` and `errors` properties. Validation never throws; all validation
failures are returned in the `errors` array.

```cjs
const { Schema } = require('node:validator');
const schema = new Schema({ type: 'string', minLength: 1 });

const good = schema.validate('hello');
console.log(good.valid); // true

const bad = schema.validate('');
console.log(bad.valid); // false
console.log(bad.errors[0].code); // 'STRING_TOO_SHORT'
```

### `schema.applyDefaults(data)`

<!-- YAML
added: REPLACEME
-->

* `data` {any} The data object to apply defaults to.
* Returns: {Object} A new object with defaults applied.

Returns a new object with default values applied for missing or `undefined`
properties. The input data is not mutated. Defaults are applied recursively
for nested object schemas.

```cjs
const { Schema } = require('node:validator');
const schema = new Schema({
  type: 'object',
  properties: {
    host: { type: 'string', default: 'localhost' },
    port: { type: 'integer', default: 3000 },
  },
});

const config = schema.applyDefaults({});
console.log(config.host); // 'localhost'
console.log(config.port); // 3000
```

### `schema.toJSON()`

<!-- YAML
added: REPLACEME
-->

* Returns: {Object} A frozen copy of the original schema definition.

Returns the original schema definition as a frozen plain object. This is
useful for composing schemas — pass the result as a sub-schema definition
in another schema:

```cjs
const { Schema } = require('node:validator');
const addressSchema = new Schema({
  type: 'object',
  properties: {
    street: { type: 'string' },
    city: { type: 'string' },
  },
});

const userSchema = new Schema({
  type: 'object',
  properties: {
    name: { type: 'string' },
    address: addressSchema.toJSON(),
  },
});
```

### Static method: `Schema.validate(definition, data)`

<!-- YAML
added: REPLACEME
-->

* `definition` {Object} A schema definition object.
* `data` {any} The value to validate.
* Returns: {ValidationResult}

Convenience method that creates a `Schema` and validates in one call.
Equivalent to `new Schema(definition).validate(data)`. The definition is
compiled on every invocation; when validating repeatedly against the same
schema, reuse a single `new Schema()` instance instead.

```cjs
const { Schema } = require('node:validator');
const result = Schema.validate({ type: 'number', minimum: 0 }, 42);
console.log(result.valid); // true
```

## Type: `ValidationResult`

<!-- YAML
added: REPLACEME
-->

The object returned by [`schema.validate()`][]. It is a frozen plain object
with the following properties:

* `valid` {boolean} `true` if the data matches the schema.
* `errors` {Object\[]} A frozen array of error objects. Empty when `valid`
  is `true`. Each error object has the following properties:
  * `path` {string} The path to the invalid value using dot notation for
    object properties and bracket notation for array indices. The root
    path is an empty string.
  * `message` {string} A human-readable error description.
  * `code` {string} A machine-readable error code from the
    [validation error codes][] table.

[`--no-experimental-validator`]: cli.md#--no-experimental-validator
[`ERR_VALIDATOR_INVALID_SCHEMA`]: errors.md#err_validator_invalid_schema
[`schema.applyDefaults()`]: #schemaapplydefaultsdata
[`schema.validate()`]: #schemavalidatedata
[validation error codes]: #validation-error-codes
