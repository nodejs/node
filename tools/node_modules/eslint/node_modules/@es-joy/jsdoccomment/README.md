# @es-joy/jsdoccomment

[![Node.js CI status](https://github.com/brettz9/getJSDocComment/workflows/Node.js%20CI/badge.svg)](https://github.com/brettz9/getJSDocComment/actions)

This project aims to preserve and expand upon the
`SourceCode#getJSDocComment` functionality of the deprecated ESLint method.

It also exports a number of functions currently for working with JSDoc:

## API

### `parseComment`

For parsing `comment-parser` in a JSDoc-specific manner.
Might wish to have tags with or without tags, etc. derived from a split off
JSON file.

### `commentParserToESTree`

Converts [comment-parser](https://github.com/syavorsky/comment-parser)
AST to ESTree/ESLint/Babel friendly AST. See the "ESLint AST..." section below.

### `jsdocVisitorKeys`

The [VisitorKeys](https://github.com/eslint/eslint-visitor-keys)
for `JsdocBlock`, `JsdocDescriptionLine`, and `JsdocTag`. More likely to be
subject to change or dropped in favor of another type parser.

### `jsdocTypeVisitorKeys`

Just a re-export of [VisitorKeys](https://github.com/eslint/eslint-visitor-keys)
from [`jsdoc-type-pratt-parser`](https://github.com/simonseyock/jsdoc-type-pratt-parser/).

### `getDefaultTagStructureForMode`

Provides info on JSDoc tags:

- `nameContents` ('namepath-referencing'|'namepath-defining'|
    'dual-namepath-referencing'|false) - Whether and how a name is allowed
    following any type. Tags without a proper name (value `false`) may still
    have a description (which can appear like a name); `descriptionAllowed`
    in such cases would be `true`.
    The presence of a truthy `nameContents` value is therefore only intended
    to signify whether separate parsing should occur for a name vs. a
    description, and what its nature should be.
- `nameRequired` (boolean) - Whether a name must be present following any type.
- `descriptionAllowed` (boolean) - Whether a description (following any name)
    is allowed.
- `typeAllowed` (boolean) - Whether the tag accepts a curly bracketed portion.
    Even without a type, a tag may still have a name and/or description.
- `typeRequired` (boolean) - Whether a curly bracketed type must be present.
- `typeOrNameRequired` (boolean) - Whether either a curly bracketed type is
    required or a name, but not necessarily both.

### Miscellaneous

Also currently exports these utilities, though they might be removed in the
future:

- `getTokenizers` - Used with `parseComment` (its main core)
- `toCamelCase` - Convert to CamelCase.
- `hasSeeWithLink` - A utility to detect if a tag is `@see` and has a `@link`
- `commentHandler` - Used by `eslint-plugin-jsdoc`. Might be removed in future.
- `commentParserToESTree`- Converts [comment-parser](https://github.com/syavorsky/comment-parser)
    AST to ESTree/ESLint/Babel friendly AST
- `jsdocVisitorKeys` - The [VisitorKeys](https://github.com/eslint/eslint-visitor-keys)
    for `JSDocBlock`, `JSDocDescriptionLine`, and `JSDocTag`. Might change.
- `jsdocTypeVisitorKeys` - [VisitorKeys](https://github.com/eslint/eslint-visitor-keys)
    for `jsdoc-type-pratt-parser`.
- `getTokenizers` - A utility. Might be removed in future.
- `toCamelCase` - A utility. Might be removed in future.
- `hasSeeWithLink` - A utility to detect if a tag is `@see` and has a `@link`
- `defaultNoTypes` = The tags which allow no types by default:
    `default`, `defaultvalue`, `see`;
- `defaultNoNames` - The tags which allow no names by default:
    `access`, `author`, `default`, `defaultvalue`, `description`, `example`,
    `exception`, `kind`, `license`, `return`, `returns`, `since`, `summary`,
    `throws`, `version`, `variation`

## ESLint AST produced for `comment-parser` nodes (`JsdocBlock`, `JsdocTag`, and `JsdocDescriptionLine`)

Note: Although not added in this package, `@es-joy/jsdoc-eslint-parser` adds
a `jsdoc` property to other ES nodes (using this project's `getJSDocComment`
to determine the specific comment-block that will be attached as AST).

### `JsdocBlock`

Has two visitable properties:

1. `tags` (an array of `JsdocTag`; see below)
2. `descriptionLines` (an array of `JsdocDescriptionLine` for multiline
    descriptions).

Has the following custom non-visitable property:

1. `lastDescriptionLine` - A number
2. `endLine` - A number representing the line number with `end`

May also have the following non-visitable properties from `comment-parser`:

1. `description` - Same as `descriptionLines` but as a string with newlines.
2. `delimiter`
3. `postDelimiter`
4. `lineEnd`
5. `end`

### `JsdocTag`

Has three visitable properties:

1. `parsedType` (the `jsdoc-type-pratt-parser` AST representation of the tag's
    type (see the `jsdoc-type-pratt-parser` section below)).
2. `descriptionLines` (an array of `JsdocDescriptionLine` for multiline
    descriptions)
3. `typeLines` (an array of `JsdocTypeLine` for multiline type strings)

May also have the following non-visitable properties from `comment-parser`
(note that all are included from `comment-parser` except `end` as that is only
for JSDoc blocks and note that `type` is renamed to `rawType`):

1. `description` - Same as `descriptionLines` but as a string with newlines.
2. `rawType` - `comment-parser` has this named as `type`, but because of a
    conflict with ESTree using `type` for Node type, we renamed it to
    `rawType`. It is otherwise the same as in `comment-parser`, i.e., a string
    with newlines, though with the initial `{` and final `}` stripped out.
    See `typeLines` for the array version of this property.
3. `start`
4. `delimiter`
5. `postDelimiter`
6. `tag` (this does differ from `comment-parser` now in terms of our stripping
    the initial `@`)
7. `postTag`
8. `name`
9. `postName`
10. `postType`

### `JsdocDescriptionLine`

No visitable properties.

May also have the following non-visitable properties from `comment-parser`:

1. `delimiter`
2. `postDelimiter`
3. `start`
4. `description`

### `JsdocTypeLine`

No visitable properties.

May also have the following non-visitable properties from `comment-parser`:

1. `delimiter`
2. `postDelimiter`
3. `start`
4. `rawType` - Renamed from `comment-parser` to avoid a conflict. See
    explanation under `JsdocTag`

## ESLint AST produced for `jsdoc-type-pratt-parser`

The AST, including `type`, remains as is from [jsdoc-type-pratt-parser](https://github.com/simonseyock/jsdoc-type-pratt-parser/).

The type will always begin with a `JsdocType` prefix added, along with a
camel-cased type name, e.g., `JsdocTypeUnion`.

The `jsdoc-type-pratt-parser` visitor keys are also preserved without change.

## Installation

```shell
npm i @es-joy/jsdoccomment
```

## Changelog

The changelog can be found on the [CHANGES.md](./CHANGES.md).
<!--## Contributing

Everyone is welcome to contribute. Please take a moment to review the [contributing guidelines](CONTRIBUTING.md).
-->
## Authors and license

[Brett Zamir](http://brett-zamir.me/) and
[contributors](https://github.com/es-joy/jsdoc-eslint-parser/graphs/contributors).

MIT License, see the included [LICENSE-MIT.txt](LICENSE-MIT.txt) file.

## To-dos

1. Get complete code coverage
