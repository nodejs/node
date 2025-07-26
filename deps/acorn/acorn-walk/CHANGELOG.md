## 8.3.4 (2024-09-09)

### Bug fixes

Walk SwitchCase nodes as separate nodes.

## 8.3.3 (2024-01-11)

### Bug fixes

Make acorn a dependency because acorn-walk uses the types from that package.

## 8.3.2 (2024-01-11)

### Bug fixes

Add missing type for `findNodeBefore`.

## 8.3.1 (2023-12-06)

### Bug fixes

Add `Function` and `Class` to the `AggregateType` type, so that they can be used in walkers without raising a type error.

Visitor functions are now called in such a way that their `this` refers to the object they are part of.

## 8.3.0 (2023-10-26)

### New features

Use a set of new, much more precise, TypeScript types.

## 8.2.0 (2021-09-06)

### New features

Add support for walking ES2022 class static blocks.

## 8.1.1 (2021-06-29)

### Bug fixes

Include `base` in the type declarations.

## 8.1.0 (2021-04-24)

### New features

Support node types for class fields and private methods.

## 8.0.2 (2021-01-25)

### Bug fixes

Adjust package.json to work with Node 12.16.0 and 13.0-13.6.

## 8.0.0 (2021-01-05)

### Bug fixes

Fix a bug where `full` and `fullAncestor` would skip nodes with overridden types.

## 8.0.0 (2020-08-12)

### New features

The package can now be loaded directly as an ECMAScript module in node 13+.

## 7.2.0 (2020-06-17)

### New features

Support optional chaining and nullish coalescing.

Support `import.meta`.

Add support for `export * as ns from "source"`.

## 7.1.1 (2020-02-13)

### Bug fixes

Clean up the type definitions to actually work well with the main parser.

## 7.1.0 (2020-02-11)

### New features

Add a TypeScript definition file for the library.

## 7.0.0 (2017-08-12)

### New features

Support walking `ImportExpression` nodes.

## 6.2.0 (2017-07-04)

### New features

Add support for `Import` nodes.

## 6.1.0 (2018-09-28)

### New features

The walker now walks `TemplateElement` nodes.

## 6.0.1 (2018-09-14)

### Bug fixes

Fix bad "main" field in package.json.

## 6.0.0 (2018-09-14)

### Breaking changes

This is now a separate package, `acorn-walk`, rather than part of the main `acorn` package.

The `ScopeBody` and `ScopeExpression` meta-node-types are no longer supported.

## 5.7.1 (2018-06-15)

### Bug fixes

Make sure the walker and bin files are rebuilt on release (the previous release didn't get the up-to-date versions).

## 5.7.0 (2018-06-15)

### Bug fixes

Fix crash in walker when walking a binding-less catch node.

## 5.6.2 (2018-06-05)

### Bug fixes

In the walker, go back to allowing the `baseVisitor` argument to be null to default to the default base everywhere.

## 5.6.1 (2018-06-01)

### Bug fixes

Fix regression when passing `null` as fourth argument to `walk.recursive`.

## 5.6.0 (2018-05-31)

### Bug fixes

Fix a bug in the walker that caused a crash when walking an object pattern spread.

## 5.5.1 (2018-03-06)

### Bug fixes

Fix regression in walker causing property values in object patterns to be walked as expressions.

## 5.5.0 (2018-02-27)

### Bug fixes

Support object spread in the AST walker.

## 5.4.1 (2018-02-02)

### Bug fixes

5.4.0 somehow accidentally included an old version of walk.js.

## 5.2.0 (2017-10-30)

### Bug fixes

The `full` and `fullAncestor` walkers no longer visit nodes multiple times.

## 5.1.0 (2017-07-05)

### New features

New walker functions `full` and `fullAncestor`.

## 3.2.0 (2016-06-07)

### New features

Make it possible to use `visit.ancestor` with a walk state.

## 3.1.0 (2016-04-18)

### New features

The walker now allows defining handlers for `CatchClause` nodes.

## 2.5.2 (2015-10-27)

### Fixes

Fix bug where the walker walked an exported `let` statement as an expression.
