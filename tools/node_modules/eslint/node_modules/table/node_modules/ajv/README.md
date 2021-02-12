<img align="right" alt="Ajv logo" width="160" src="https://ajv.js.org/images/ajv_logo.png">

# Ajv: Another JSON schema validator

The fastest JSON schema validator for Node.js and browser. Supports JSON Schema draft-06/07/2019-09 (draft-04 is supported in [version 6](https://github.com/ajv-validator/ajv/tree/v6)) and JSON Type Definition [RFC8927](https://datatracker.ietf.org/doc/rfc8927/).

[![build](https://github.com/ajv-validator/ajv/workflows/build/badge.svg)](https://github.com/ajv-validator/ajv/actions?query=workflow%3Abuild)
[![npm](https://img.shields.io/npm/v/ajv.svg)](https://www.npmjs.com/package/ajv)
[![npm downloads](https://img.shields.io/npm/dm/ajv.svg)](https://www.npmjs.com/package/ajv)
[![Coverage Status](https://coveralls.io/repos/github/ajv-validator/ajv/badge.svg?branch=master)](https://coveralls.io/github/ajv-validator/ajv?branch=master)
[![Gitter](https://img.shields.io/gitter/room/ajv-validator/ajv.svg)](https://gitter.im/ajv-validator/ajv)
[![GitHub Sponsors](https://img.shields.io/badge/$-sponsors-brightgreen)](https://github.com/sponsors/epoberezkin)

## Platinum sponsors

[<img src="https://www.poberezkin.com/images/mozilla.svg" width="45%">](https://www.mozilla.org)[<img src="https://ajv.js.org/images/gap.svg" width="9%">](https://opencollective.com/ajv)[<img src="https://ajv.js.org/images/reserved.svg" width="45%">](https://opencollective.com/ajv)

## Using version 7

Ajv version 7 has these new features:

- support of JSON Schema draft-2019-09 features: [`unevaluatedProperties`](./docs/json-schema.md#unevaluatedproperties) and [`unevaluatedItems`](./docs/json-schema.md#unevaluateditems), [dynamic recursive references](./docs/validation.md#extending-recursive-schemas) and other [additional keywords](./docs/json-schema.md#json-schema-draft-2019-09).
- NEW: support of JSON Type Definition [RFC8927](https://datatracker.ietf.org/doc/rfc8927/) (from [v7.1.0](https://github.com/ajv-validator/ajv-keywords/releases/tag/v7.1.0))
- to reduce the mistakes in JSON schemas and unexpected validation results, [strict mode](./docs/strict-mode.md) is added - it prohibits ignored or ambiguous JSON Schema elements.
- to make code injection from untrusted schemas impossible, [code generation](./docs/codegen.md) is fully re-written to be safe and to allow code optimization (compiled schema code size is reduced by more than 10%).
- to simplify Ajv extensions, the new keyword API that is used by pre-defined keywords is available to user-defined keywords - it is much easier to define any keywords now, especially with subschemas. [ajv-keywords](https://github.com/ajv-validator/ajv-keywords) package was updated to use the new API (in [v4.0.0](https://github.com/ajv-validator/ajv-keywords/releases/tag/v4.0.0))
- schemas are compiled to ES6 code (ES5 code generation is also supported with an option).
- to improve reliability and maintainability the code is migrated to TypeScript.

**Please note**:

- the support for JSON-Schema draft-04 is removed - if you have schemas using "id" attributes you have to replace them with "\$id" (or continue using [Ajv v6](https://github.com/ajv-validator/ajv/tree/v6) that will be supported until 02/28/2021).
- all formats are separated to ajv-formats package - they have to be explicitly added if you use them.

See [release notes](https://github.com/ajv-validator/ajv/releases/tag/v7.0.0) for the details.

To install the new version:

```bash
npm install ajv
```

See [Getting started](#usage) for code example.

## Contributing

100+ people contributed to Ajv. You are very welcome to join by implementing new features that are valuable to many users and by improving documentation.

Please do not be disappointed if your suggestion is not accepted - it is important to maintain the balance between the library size, performance and functionality. If it seems that a feature would benefit only a small number of users, its addition may be delayed until there is more support from the users community - so please submit the issue first to explain why this feature is important.

Please include documentation and test coverage with any new feature implementations.

To run tests:

```bash
npm install
git submodule update --init
npm test
```

`npm run build` - compiles typescript to `dist` folder.

Please review [Contributing guidelines](./CONTRIBUTING.md) and [Code components](./docs/components.md).

## Contents

- [Platinum sponsors](#platinum-sponsors)
- [Using version 7](#using-version-7)
- [Contributing](#contributing)
- [Mozilla MOSS grant and OpenJS Foundation](#mozilla-moss-grant-and-openjs-foundation)
- [Sponsors](#sponsors)
- [Performance](#performance)
- [Features](#features)
- [Getting started](#usage)
- [Choosing schema language](#choosing-schema-language)
  - [JSON Schema](#json-schema)
  - [JSON Type Definition](#json-type-definition)
- [Frequently Asked Questions](./docs/faq.md)
- [Using in browser](#using-in-browser)
  - [Content Security Policy](./docs/security.md#content-security-policy)
- [Using in ES5 environment](#using-in-es5-environment)
- [Command line interface](#command-line-interface)
- [API reference](./docs/api.md)
  - [Methods](./docs/api.md#ajv-constructor-and-methods)
  - [Options](./docs/api.md#options)
  - [Validation errors](./docs/api.md#validation-errors)
- NEW: [Strict mode](./docs/strict-mode.md#strict-mode)
  - [Prohibit ignored keywords](./docs/strict-mode.md#prohibit-ignored-keywords)
  - [Prevent unexpected validation](./docs/strict-mode.md#prevent-unexpected-validation)
  - [Strict types](./docs/strict-mode.md#strict-types)
  - [Strict number validation](./docs/strict-mode.md#strict-number-validation)
- [Data validation](./docs/validation.md)
  - [Validation basics](./docs/validation.md#validation-basics): [JSON Schema keywords](./docs/validation.md#validation-keywords), [annotations](./docs/validation.md#annotation-keywords), [formats](./docs/validation.md#formats)
  - [Modular schemas](./docs/validation.md#modular-schemas): [combining with \$ref](./docs/validation.md#ref), [\$data reference](./docs/validation.md#data-reference), [$merge and $patch](./docs/validation.md#merge-and-patch-keywords)
  - [Asynchronous schema compilation](./docs/validation.md#asynchronous-schema-compilation)
  - [Standalone validation code](./docs/standalone.md)
  - [Asynchronous validation](./docs/validation.md#asynchronous-validation)
  - [Modifying data](./docs/validation.md#modifying-data-during-validation): [additional properties](./docs/validation.md#removing-additional-properties), [defaults](./docs/validation.md#assigning-defaults), [type coercion](./docs/validation.md#coercing-data-types)
- [Extending Ajv](#extending-ajv)
  - User-defined keywords:
    - [basics](./docs/validation.md#user-defined-keywords)
    - [guide](./docs/keywords.md)
  - [Plugins](#plugins)
  - [Related packages](#related-packages)
- [Security considerations](./docs/security.md)
  - [Security contact](./docs/security.md#security-contact)
  - [Untrusted schemas](./docs/security.md#untrusted-schemas)
  - [Circular references in objects](./docs/security.md#circular-references-in-javascript-objects)
  - [Trusted schemas](./docs/security.md#security-risks-of-trusted-schemas)
  - [ReDoS attack](./docs/security.md#redos-attack)
  - [Content Security Policy](./docs/security.md#content-security-policy)
- [Some packages using Ajv](#some-packages-using-ajv)
- [Changes history](#changes-history)
- [Support, Code of conduct, Contacts, License](#open-source-software-support)

## Mozilla MOSS grant and OpenJS Foundation

[<img src="https://www.poberezkin.com/images/mozilla.png" width="240" height="68">](https://www.mozilla.org/en-US/moss/) &nbsp;&nbsp;&nbsp; [<img src="https://www.poberezkin.com/images/openjs.png" width="220" height="68">](https://openjsf.org/blog/2020/08/14/ajv-joins-openjs-foundation-as-an-incubation-project/)

Ajv has been awarded a grant from Mozilla’s [Open Source Support (MOSS) program](https://www.mozilla.org/en-US/moss/) in the “Foundational Technology” track! It will sponsor the development of Ajv support of [JSON Schema version 2019-09](https://tools.ietf.org/html/draft-handrews-json-schema-02) and of [JSON Type Definition (RFC8927)](https://datatracker.ietf.org/doc/rfc8927/).

Ajv also joined [OpenJS Foundation](https://openjsf.org/) – having this support will help ensure the longevity and stability of Ajv for all its users.

This [blog post](https://www.poberezkin.com/posts/2020-08-14-ajv-json-validator-mozilla-open-source-grant-openjs-foundation.html) has more details.

I am looking for the long term maintainers of Ajv – working with [ReadySet](https://www.thereadyset.co/), also sponsored by Mozilla, to establish clear guidelines for the role of a "maintainer" and the contribution standards, and to encourage a wider, more inclusive, contribution from the community.

## <a name="sponsors"></a>Please [sponsor Ajv development](https://github.com/sponsors/epoberezkin)

Since I asked to support Ajv development 40 people and 6 organizations contributed via GitHub and OpenCollective - this support helped receiving the MOSS grant!

Your continuing support is very important - the funds will be used to develop and maintain Ajv once the next major version is released.

Please sponsor Ajv via:

- [GitHub sponsors page](https://github.com/sponsors/epoberezkin) (GitHub will match it)
- [Ajv Open Collective️](https://opencollective.com/ajv)

Thank you.

#### Open Collective sponsors

<a href="https://opencollective.com/ajv"><img src="https://opencollective.com/ajv/individuals.svg?width=890"></a>

<a href="https://opencollective.com/ajv/organization/0/website"><img src="https://opencollective.com/ajv/organization/0/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/1/website"><img src="https://opencollective.com/ajv/organization/1/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/2/website"><img src="https://opencollective.com/ajv/organization/2/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/3/website"><img src="https://opencollective.com/ajv/organization/3/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/4/website"><img src="https://opencollective.com/ajv/organization/4/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/5/website"><img src="https://opencollective.com/ajv/organization/5/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/6/website"><img src="https://opencollective.com/ajv/organization/6/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/7/website"><img src="https://opencollective.com/ajv/organization/7/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/8/website"><img src="https://opencollective.com/ajv/organization/8/avatar.svg"></a>
<a href="https://opencollective.com/ajv/organization/9/website"><img src="https://opencollective.com/ajv/organization/9/avatar.svg"></a>

## Performance

Ajv generates code to turn JSON Schemas into super-fast validation functions that are efficient for v8 optimization.

Currently Ajv is the fastest and the most standard compliant validator according to these benchmarks:

- [json-schema-benchmark](https://github.com/ebdrup/json-schema-benchmark) - 50% faster than the second place
- [jsck benchmark](https://github.com/pandastrike/jsck#benchmarks) - 20-190% faster
- [z-schema benchmark](https://rawgit.com/zaggino/z-schema/master/benchmark/results.html)
- [themis benchmark](https://cdn.rawgit.com/playlyfe/themis/master/benchmark/results.html)

Performance of different validators by [json-schema-benchmark](https://github.com/ebdrup/json-schema-benchmark):

[![performance](https://chart.googleapis.com/chart?chxt=x,y&cht=bhs&chco=76A4FB&chls=2.0&chbh=32,4,1&chs=600x416&chxl=-1:|djv|ajv|json-schema-validator-generator|jsen|is-my-json-valid|themis|z-schema|jsck|skeemas|json-schema-library|tv4&chd=t:100,98,72.1,66.8,50.1,15.1,6.1,3.8,1.2,0.7,0.2)](https://github.com/ebdrup/json-schema-benchmark/blob/master/README.md#performance)

## Features

- Ajv implements JSON Schema [draft-06/07/2019-09](http://json-schema.org/) standards (draft-04 is supported in v6):
  - all validation keywords (see [JSON Schema validation keywords](./docs/json-schema.md))
  - keyword "nullable" from [Open API 3 specification](https://swagger.io/docs/specification/data-models/data-types/).
  - full support of remote references (remote schemas have to be added with `addSchema` or compiled to be available)
  - support of circular references between schemas
  - correct string lengths for strings with unicode pairs
  - [formats](#formats) defined by JSON Schema draft-07 standard (with [ajv-formats](https://github.com/ajv-validator/ajv-formats) plugin) and additional formats (can be turned off)
  - [validates schemas against meta-schema](./docs/api.md#api-validateschema)
- NEW: supports [JSON Type Definition](https://datatracker.ietf.org/doc/rfc8927/):
  - all forms (see [JSON Type Definition schema forms](./docs/json-type-definition.md))
  - meta-schema for JTD schemas
  - "union" keyword and user-defined keywords (can be used inside "metadata" member of the schema)
- supports [browsers](#using-in-browser) and Node.js 0.10-14.x
- [asynchronous loading](./docs/validation.md#asynchronous-schema-compilation) of referenced schemas during compilation
- "All errors" validation mode with [option allErrors](./docs/api.md#options)
- [error messages with parameters](./docs/api.md#validation-errors) describing error reasons to allow error message generation
- i18n error messages support with [ajv-i18n](https://github.com/ajv-validator/ajv-i18n) package
- [removing-additional-properties](./docs/validation.md#removing-additional-properties)
- [assigning defaults](./docs/validation.md#assigning-defaults) to missing properties and items
- [coercing data](./docs/validation.md#coercing-data-types) to the types specified in `type` keywords
- [user-defined keywords](#user-defined-keywords)
- draft-06/07 keywords `const`, `contains`, `propertyNames` and `if/then/else`
- draft-06 boolean schemas (`true`/`false` as a schema to always pass/fail).
- additional extension keywords with [ajv-keywords](https://github.com/ajv-validator/ajv-keywords) package
- [\$data reference](./docs/validation.md#data-reference) to use values from the validated data as values for the schema keywords
- [asynchronous validation](./docs/api.md#asynchronous-validation) of user-defined formats and keywords

## Install

To install version 7:

```
npm install ajv
```

## <a name="usage"></a>Getting started

Try it in the Node.js REPL: https://runkit.com/npm/ajv

In JavaScript:

```javascript
// or ESM/TypeScript import
import Ajv from "ajv"
// Node.js require:
const Ajv = require("ajv").default

const ajv = new Ajv() // options can be passed, e.g. {allErrors: true}
const validate = ajv.compile(schema)
const valid = validate(data)
if (!valid) console.log(validate.errors)
```

In TypeScript:

```typescript
import Ajv, {JSONSchemaType, DefinedError} from "ajv"

const ajv = new Ajv()

type MyData = {foo: number}

// Optional schema type annotation for schema to match MyData type.
// To use JSONSchemaType set `strictNullChecks: true` in tsconfig `compilerOptions`.
const schema: JSONSchemaType<MyData> = {
  type: "object",
  properties: {
    foo: {type: "number", minimum: 0},
  },
  required: ["foo"],
  additionalProperties: false,
}

// validate is a type guard for MyData - type is inferred from schema type
const validate = ajv.compile(schema)

// or, if you did not use type annotation for the schema,
// type parameter can be used to make it type guard:
// const validate = ajv.compile<MyData>(schema)

const data: any = {foo: 1}

if (validate(data)) {
  // data is MyData here
  console.log(data.foo)
} else {
  // The type cast is needed to allow user-defined keywords and errors
  // You can extend this type to include your error types as needed.
  for (const err of validate.errors as DefinedError[]) {
    switch (err.keyword) {
      case "minimum":
        // err type is narrowed here to have "minimum" error params properties
        console.log(err.params.limit)
        break
      // ...
    }
  }
}
```

With JSON Type Definition schema:

```javascript
const Ajv = require("ajv").default

const ajv = new Ajv() // options can be passed, e.g. {allErrors: true}
const schema = {
  properties: {
    foo: {type: "float64"},
  },
}
const validate = ajv.compile(schema)
const valid = validate({foo: 1}) // true
if (!valid) console.log(validate.errors)
// Unlike JSON Schema:
const valid1 = validate(1) // false, bot an object
const valid2 = validate({}) // false, foo is required
const valid3 = validate({foo: 1, bar: 2}) // false, bar is additional
```

See [this test](./spec/types/json-schema.spec.ts) for an advanced example, [API reference](./docs/api.md) and [Options](./docs/api.md#options) for more details.

Ajv compiles schemas to functions and caches them in all cases (using schema itself as a key for Map) or another function passed via options), so that the next time the same schema is used (not necessarily the same object instance) it won't be compiled again.

The best performance is achieved when using compiled functions returned by `compile` or `getSchema` methods (there is no additional function call).

**Please note**: every time a validation function or `ajv.validate` are called `errors` property is overwritten. You need to copy `errors` array reference to another variable if you want to use it later (e.g., in the callback). See [Validation errors](./docs/api.md#validation-errors)

## Using in browser

See [Content Security Policy](./docs/security.md#content-security-policy) to decide the best approach how to use Ajv in the browser.

Whether you use Ajv or compiled schemas, it is recommended that you bundle them together with your code.

If you need to use Ajv in several bundles you can create a separate UMD bundles using `npm run bundle` script.

Then you need to load Ajv with support of JSON Schema draft-07 in the browser:

```html
<script src="bundle/ajv7.min.js"></script>
<script>
  ;(function () {
    const Ajv = window.ajv7.default
    const ajv = new Ajv()
  })()
</script>
```

To load the bundle that supports JSON Schema draft-2019-09:

```html
<script src="bundle/ajv2019.min.js"></script>
<script>
  ;(function () {
    const Ajv = window.ajv2019.default
    const ajv = new Ajv()
  })()
</script>
```

To load the bundle that supports JSON Type Definition:

```html
<script src="bundle/ajvJTD.min.js"></script>
<script>
  ;(function () {
    const Ajv = window.ajvJTD.default
    const ajv = new Ajv()
  })()
</script>
```

This bundle can be used with different module systems; it creates global `ajv` (or `ajv2019`) if no module system is found.

The browser bundle is available on [cdnjs](https://cdnjs.com/libraries/ajv).

**Please note**: some frameworks, e.g. Dojo, may redefine global require in a way that is not compatible with CommonJS module format. In this case Ajv bundle has to be loaded before the framework and then you can use global `ajv` (see issue [#234](https://github.com/ajv-validator/ajv/issues/234)).

## Choosing schema language

Both JSON Schema and JSON Type Definition are cross-platform specifications with implementations in multiple programming languages that help you define the shape and requirements to your JSON data.

This section compares their pros/cons to help decide which specification fits your application better.

### JSON Schema

- Pros
  - Wide specification adoption.
  - Used as part of OpenAPI specification.
  - Support of complex validation scenarios:
    - untagged unions and boolean logic
    - conditional schemas and dependencies
    - restrictions on the number ranges and the size of strings, arrays and objects
    - semantic validation with formats, patterns and content keywords
    - distribute strict record definitions across multiple schemas (with unevaluatedProperties)
  - Can be effectively used for validation of any JavaScript objects and configuration files.
- Cons
  - Defines the collection of restrictions on the data, rather than the shape of the data.
  - No standard support for tagged unions.
  - Complex and error prone for the new users (Ajv has [strict mode](./docs/strict-mode) to compensate for it, but it is not cross-platform).
  - Some parts of specification are difficult to implement, creating the risk of implementations divergence:
    - reference resolution model
    - unevaluatedProperties/unevaluatedItems
    - dynamic recursive references
  - Internet draft status (rather than RFC)

See [JSON Schema](./docs/json-schema.md) for the list of defined keywords.

### JSON Type Definition

- Pros:
  - Aligned with type systems of many languages - can be used to generate type definitions and efficient parsers and serializers to/from these types.
  - Very simple, enforcing the best practices for cross-platform JSON API modelling.
  - Simple to implement, ensuring consistency across implementations.
  - Defines the shape of JSON data via strictly defined schema forms (rather than the collection of restrictions).
  - Effective support for tagged unions.
  - Designed to protect against user mistakes.
  - Approved as [RFC8927](https://datatracker.ietf.org/doc/rfc8927/)
- Cons:
  - Limited, compared with JSON Schema - no support for untagged unions<sup>\*</sup>, conditionals, references between different schema files<sup>\*\*</sup>, etc.
  - No meta-schema in the specification<sup>\*</sup>.
  - Brand new - limited industry adoption (as of January 2021).

<sup>\*</sup> Ajv defines meta-schema for JTD schemas and non-standard keyword "union" that can be used inside "metadata" object.

<sup>\*\*</sup> You can still combine schemas from multiple files in the application code.

See [JSON Type Definition](./docs/json-type-definition.md) for the list of defined schema forms.

## Using in ES5 environment

You need to:

- recompile Typescript to ES5 target - it is set to 2018 in the bundled compiled code.
- generate ES5 validation code:

```javascript
const ajv = new Ajv({code: {es5: true}})
```

See [Advanced options](https://github.com/ajv-validator/ajv/blob/master/docs/api.md#advanced-options).

## Command line interface

CLI is available as a separate npm package [ajv-cli](https://github.com/ajv-validator/ajv-cli). It supports:

- compiling JSON Schemas to test their validity
- generating [standalone validation code](./docs/standalone.md) that exports validation function(s) to be used without Ajv
- migrating schemas to draft-07 and draft-2019-09 (using [json-schema-migrate](https://github.com/epoberezkin/json-schema-migrate))
- validating data file(s) against JSON Schema
- testing expected validity of data against JSON Schema
- referenced schemas
- user-defined meta-schemas, validation keywords and formats
- files in JSON, JSON5, YAML, and JavaScript format
- all Ajv options
- reporting changes in data after validation in [JSON-patch](https://datatracker.ietf.org/doc/rfc6902/) format

## Extending Ajv

### User defined keywords

See section in [data validation](./docs/validation.md#user-defined-keywords) and the [detailed guide](./docs/keywords.md).

### Plugins

Ajv can be extended with plugins that add keywords, formats or functions to process generated code. When such plugin is published as npm package it is recommended that it follows these conventions:

- it exports a function that accepts ajv instance as the first parameter - it allows using plugins with [ajv-cli](#command-line-interface).
- this function returns the same instance to allow chaining.
- this function can accept an optional configuration as the second parameter.

You can import `Plugin` interface from ajv if you use Typescript.

If you have published a useful plugin please submit a PR to add it to the next section.

### Related packages

- [ajv-bsontype](https://github.com/BoLaMN/ajv-bsontype) - plugin to validate mongodb's bsonType formats
- [ajv-cli](https://github.com/jessedc/ajv-cli) - command line interface
- [ajv-formats](https://github.com/ajv-validator/ajv-formats) - formats defined in JSON Schema specification
- [ajv-errors](https://github.com/ajv-validator/ajv-errors) - plugin for defining error messages in the schema
- [ajv-i18n](https://github.com/ajv-validator/ajv-i18n) - internationalised error messages
- [ajv-istanbul](https://github.com/ajv-validator/ajv-istanbul) - plugin to instrument generated validation code to measure test coverage of your schemas
- [ajv-keywords](https://github.com/ajv-validator/ajv-keywords) - plugin with additional validation keywords (select, typeof, etc.)
- [ajv-merge-patch](https://github.com/ajv-validator/ajv-merge-patch) - plugin with keywords $merge and $patch
- [ajv-formats-draft2019](https://github.com/luzlab/ajv-formats-draft2019) - format validators for draft2019 that aren't included in [ajv-formats](https://github.com/ajv-validator/ajv-formats) (ie. `idn-hostname`, `idn-email`, `iri`, `iri-reference` and `duration`)

## Some packages using Ajv

- [webpack](https://github.com/webpack/webpack) - a module bundler. Its main purpose is to bundle JavaScript files for usage in a browser
- [jsonscript-js](https://github.com/JSONScript/jsonscript-js) - the interpreter for [JSONScript](http://www.jsonscript.org) - scripted processing of existing endpoints and services
- [osprey-method-handler](https://github.com/mulesoft-labs/osprey-method-handler) - Express middleware for validating requests and responses based on a RAML method object, used in [osprey](https://github.com/mulesoft/osprey) - validating API proxy generated from a RAML definition
- [har-validator](https://github.com/ahmadnassri/har-validator) - HTTP Archive (HAR) validator
- [jsoneditor](https://github.com/josdejong/jsoneditor) - a web-based tool to view, edit, format, and validate JSON http://jsoneditoronline.org
- [JSON Schema Lint](https://github.com/nickcmaynard/jsonschemalint) - a web tool to validate JSON/YAML document against a single JSON Schema http://jsonschemalint.com
- [objection](https://github.com/vincit/objection.js) - SQL-friendly ORM for Node.js
- [table](https://github.com/gajus/table) - formats data into a string table
- [ripple-lib](https://github.com/ripple/ripple-lib) - a JavaScript API for interacting with [Ripple](https://ripple.com) in Node.js and the browser
- [restbase](https://github.com/wikimedia/restbase) - distributed storage with REST API & dispatcher for backend services built to provide a low-latency & high-throughput API for Wikipedia / Wikimedia content
- [hippie-swagger](https://github.com/CacheControl/hippie-swagger) - [Hippie](https://github.com/vesln/hippie) wrapper that provides end to end API testing with swagger validation
- [react-form-controlled](https://github.com/seeden/react-form-controlled) - React controlled form components with validation
- [rabbitmq-schema](https://github.com/tjmehta/rabbitmq-schema) - a schema definition module for RabbitMQ graphs and messages
- [@query/schema](https://www.npmjs.com/package/@query/schema) - stream filtering with a URI-safe query syntax parsing to JSON Schema
- [chai-ajv-json-schema](https://github.com/peon374/chai-ajv-json-schema) - chai plugin to us JSON Schema with expect in mocha tests
- [grunt-jsonschema-ajv](https://github.com/SignpostMarv/grunt-jsonschema-ajv) - Grunt plugin for validating files against JSON Schema
- [extract-text-webpack-plugin](https://github.com/webpack-contrib/extract-text-webpack-plugin) - extract text from bundle into a file
- [electron-builder](https://github.com/electron-userland/electron-builder) - a solution to package and build a ready for distribution Electron app
- [addons-linter](https://github.com/mozilla/addons-linter) - Mozilla Add-ons Linter
- [gh-pages-generator](https://github.com/epoberezkin/gh-pages-generator) - multi-page site generator converting markdown files to GitHub pages
- [ESLint](https://github.com/eslint/eslint) - the pluggable linting utility for JavaScript and JSX
- [Spectral](https://github.com/stoplightio/spectral) - the customizable linting utility for JSON/YAML, OpenAPI, AsyncAPI, and JSON Schema

## Changes history

See https://github.com/ajv-validator/ajv/releases

**Please note**: [Changes in version 7.0.0](https://github.com/ajv-validator/ajv/releases/tag/v7.0.0)

[Version 6.0.0](https://github.com/ajv-validator/ajv/releases/tag/v6.0.0).

## Code of conduct

Please review and follow the [Code of conduct](./CODE_OF_CONDUCT.md).

Please report any unacceptable behaviour to ajv.validator@gmail.com - it will be reviewed by the project team.

## Security contact

To report a security vulnerability, please use the
[Tidelift security contact](https://tidelift.com/security).
Tidelift will coordinate the fix and disclosure. Please do NOT report security vulnerabilities via GitHub issues.

## Open-source software support

Ajv is a part of [Tidelift subscription](https://tidelift.com/subscription/pkg/npm-ajv?utm_source=npm-ajv&utm_medium=referral&utm_campaign=readme) - it provides a centralised support to open-source software users, in addition to the support provided by software maintainers.

## License

[MIT](./LICENSE)
