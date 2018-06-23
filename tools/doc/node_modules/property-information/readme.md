# property-information [![Build Status][build-badge]][build-status] [![Coverage Status][coverage-badge]][coverage-status]

Information for HTML properties.

## Installation

[npm][]:

```bash
npm install property-information
```

## Usage

```js
var info = require('property-information');

console.log(info('class'));
```

Yields:

```json
{
  "name": "class",
  "propertyName": "className",
  "mustUseAttribute": true,
  "mustUseProperty": false,
  "boolean": false,
  "overloadedBoolean": false,
  "numeric": false,
  "positiveNumeric": false,
  "commaSeparated": false,
  "spaceSeparated": true
}
```

## API

### `propertyInformation(name)`

Get information for a DOM property by (case-insensitive) name.

Returns an [`Information`][information] object, if available.

### `propertyInformation.all`

`Object` mapping case-insensitive names to [`Information`][information]
objects.  This gives raw access to the information returned by
[`propertyInformation()`][property-information]: do not change the
objects.

### `Information`

`Object`:

*   `name` (`string`)
    — Case-insensitive name
*   `propertyName` (`string`)
    — Case-sensitive IDL attribute (e.g., a `class` attribute is added in HTML
    and a `className` is added in Web IDL)
*   `mustUseAttribute` (`boolean`)
    — Whether `setAttribute` must be used when patching a DOM node
*   `mustUseProperty` (`boolean`)
    — Whether `node[propertyName]` must be used when patching a DOM node
*   `boolean` (`boolean`)
    — Whether the value of the property is `boolean`
*   `overloadedBoolean` (`boolean`)
    — Whether the value of the property can be `boolean`
*   `numeric` (`boolean`)
    — Whether the value of the property is `number`
*   `positiveNumeric` (`boolean`)
    — Whether the value of the property is `number` and positive
*   `spaceSeparated` (`boolean`)
    — Whether the value of the property is a [space-separated][] list
*   `commaSeparated` (`boolean`)
    — Whether the value of the property is a [comma-separated][] list

Note that some values can be both `*Separated` _and_ a primitive, in that case
each of the tokens should be regarded as a primitive.  For example, `itemScope`
is both `spaceSeparated` and `boolean`:

```json
{
  "name": "itemscope",
  "propertyName": "itemScope",
  "mustUseAttribute": true,
  "mustUseProperty": false,
  "boolean": true,
  "overloadedBoolean": false,
  "numeric": false,
  "positiveNumeric": false,
  "commaSeparated": false,
  "spaceSeparated": true
}
```

## License

[MIT][license] © [Titus Wormer][author]

Derivative work based on [React][source] licensed under
[BSD-3-Clause-Clear][source-license], © 2013-2015, Facebook, Inc.

[build-badge]: https://img.shields.io/travis/wooorm/property-information.svg?style=flat

[build-status]: https://travis-ci.org/wooorm/property-information

[coverage-badge]: https://img.shields.io/codecov/c/github/wooorm/property-information.svg

[coverage-status]: https://codecov.io/github/wooorm/property-information

[npm]: https://docs.npmjs.com/cli/install

[author]: http://wooorm.com

[license]: LICENSE

[source]: https://github.com/facebook/react/blob/f445dd9/src/renderers/dom/shared/HTMLDOMPropertyConfig.js

[source-license]: https://github.com/facebook/react/blob/88cdc27/LICENSE

[space-separated]: https://html.spec.whatwg.org/#space-separated-tokens

[comma-separated]: https://html.spec.whatwg.org/#comma-separated-tokens

[information]: #information

[property-information]: #propertyinformationname
