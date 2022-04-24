# CHANGES for `@es-joy/jsdoccomment`

## 0.22.1

- fix: ensure `getJSDocComment` does not treat block comments as JSDoc unless
    their first asterisk is followed by whitespace

## 0.22.0

- fix: update dep. `jsdoc-type-pratt-parser`
- chore: update `comment-parser` and simplify as possible

## 0.21.2

- fix: only throw if the raw type is not empty

## 0.21.1

- fix: provide clearer error message for `throwOnTypeParsingErrors`

## 0.21.0

- feat: add `throwOnTypeParsingErrors` to receive run-time type parsing errors
    for `parsedType`
- chore: update jsdoc-type-pratt-parser and devDeps.; also lints

## 0.20.1

- fix: resume catching bad parsed type (at least until
    `jsdoc-type-pratt-parser` may support all expected types)

## 0.20.0

- feat: add estree stringifer
- fix: properly supports `name`/`postName` for multi-line type
- fix: allow pratt parser to fail (unless empty)
- fix: don't add tag postDelimiter when on 0 description line
- fix: avoid adding extra line when only name and no succeeding description
- docs: clarify re: `kind`
- test: add `parsedType` with correct mode; add tests
- chore: updates jsdoc-type-pratt-parser
- chore: updates devDeps.

## 0.19.0

### User-impacting

- feat: treat `@kind` as having no name

### Dev-impacting

- docs: jsdoc
- test: begin checking `jsdoccomment`
- test: adds lcov reporter and open script for it
- chore: update devDeps.

## 0.18.0

### User-impacting

- feat: add non-visitable `endLine` property (so can detect line number
    when no description present)
- feat: supply `indent` default for `parseComment`
- fix: ensure `postName` gets a space for `@template` with a description
- fix: converting JSDoc comment with tag on same line as end (e.g., single
    line) to AST
- chore: update `jsdoc-type-pratt-parser`

### Dev-impacting

- docs: add jsdoc blocks internally
- chore: update devDeps.
- test: avoid need for `expect`
- test: complete coverage for `commentHandler`, `parseComment` tests

## 0.17.0

### User-impacting

- Enhancement: Re-export `jsdoc-type-pratt-parser`
- Update: `jsdoc-type-pratt-parser` to 2.2.1

### Dev-impacting

- npm: Update devDeps.

## 0.16.0

### User-impacting

- Update: `jsdoc-type-pratt-parser` to 2.2.0

### Dev-impacting

- npm: Update devDeps.

## 0.15.0

### User-impacting

- Update: `jsdoc-type-pratt-parser` to 2.1.0

### Dev-impacting

- npm: Update devDeps.

## 0.14.2

### User-impacting

- Fix: Find comments previous to parentheses (used commonly in TypeScript)

### Dev-impacting

- npm: Update devDeps.

## 0.14.1

### User-impacting

- Update: `jsdoc-type-pratt-parser` to 2.0.2

## 0.14.0

### User-impacting

- Update: `jsdoc-type-pratt-parser` to 2.0.1

### Dev-impacting

- npm: Update devDeps.

## 0.13.0

### User-impacting

- Update: `comment-parser` to 1.3.0
- Fix: Allow comment on `ExportDefaultDeclaration`

## 0.12.0

### User-impacting

- Update: `jsdoc-type-pratt-parser` to 2.0.0
- Enhancement: Support Node 17 (@timgates42)
- Docs: Typo (@timgates42)

### Dev-impacting

- Linting: As per latest ash-nazg
- npm: Update devDeps.

## 0.11.0

- Update: For `@typescript/eslint-parser@5`, add `PropertyDefinition`

## 0.10.8

### User-impacting

- npm: Liberalize `engines` as per `comment-parser` change
- npm: Bump `comment-parser`

### Dev-impacting

- Linting: As per latest ash-nazg
- npm: Update devDeps.

## 0.10.7

- npm: Update comment-parser with CJS fix and re-exports
- npm: Update devDeps.

## 0.10.6

- Fix: Ensure copying latest build of `comment-parser`'s ESM utils

## 0.10.5

- npm: Bump fixed `jsdoc-type-pratt-parser` and devDeps.

## 0.10.4

- Fix: Bundle `comment-parser` nested imports so that IDEs (like Atom)
    bundling older Node versions can still work. Still mirroring the
    stricter `comment-parser` `engines` for now, however.

## 0.10.3

- npm: Avoid exporting nested subpaths for sake of older Node versions

## 0.10.2

- npm: Specify exact supported range: `^12.20 || ^14.14.0 || ^16`

## 0.10.1

- npm: Apply patch version of `comment-parser`

## 0.10.0

- npm: Point to stable `comment-parser`

## 0.9.0-alpha.6

### User-impacting

- Update: For `comment-parser` update, add `lineEnd`

## 0.9.0-alpha.5

### User-impacting

- npm: Bump `comment-parser` (for true ESM)
- Update: Remove extensions for packages for native ESM in `comment-parser` fix

### Dev-impacting

- npm: Update devDeps.

## 0.9.0-alpha.4

- Docs: Update repo info in `package.json`

## 0.9.0-alpha.3

- Fix: Due to `comment-parser` still needing changes, revert for now to alpha.1

## 0.9.0-alpha.2

### User-impacting

- npm: Bump `comment-parser` (for true ESM)
- Update: Remove extensions for packages for native ESM in `comment-parser` fix

### Dev-impacting

- npm: Update devDeps.

## 0.9.0-alpha.1

### User-impacting

- Breaking change: Indicate minimum for `engines` as Node >= 12
- npm: Bump `comment-parser`

### Dev-impacting

- npm: Lint cjs files
- npm: Fix eslint script
- npm: Update devDeps.

## 0.8.0

### User-impacting

- npm: Update `jsdoc-type-pratt-parser` (prerelease to stable patch)

### Dev-impacting

- npm: Update devDeps.

## 0.8.0-alpha.2

- Fix: Avoid erring with missing `typeLines`

## 0.8.0-alpha.1

- Breaking change: Export globally as `JsdocComment`
- Breaking change: Change `JSDoc` prefixes of all node types to `Jsdoc`
- Breaking change: Drop `jsdoctypeparserToESTree`
- Breaking enhancement: Switch to `jsdoc-type-pratt-parser` (toward greater
    TypeScript expressivity and compatibility/support with catharsis)
- Enhancement: Export `jsdocTypeVisitorKeys` (from `jsdoc-type-pratt-parser`)

## 0.7.2

- Fix: Add `@description` to `noNames`

## 0.7.1

- Fix: Add `@summary` to `noNames`

## 0.7.0

- Enhancement: Allow specifying `noNames` and `noTypes` on `parseComment`
    to override (or add to) tags which should have no names or types.
- Enhancement: Export `hasSeeWithLink` utility and `defaultNoTypes` and
    `defaultNoNames`.

## 0.6.0

- Change `comment-parser` `tag` AST to avoid initial `@`

## 0.5.1

- Fix: Avoid setting `variation` name (just the description) (including in
    dist)
- npm: Add `prepublishOnly` script

## 0.5.0

- Fix: Avoid setting `variation` name (just the description)

## 0.4.4

- Fix: Avoid setting `name` and `description` for simple `@template SomeName`

## 0.4.3

- npm: Ignores Github file

## 0.4.2

- Fix: Ensure replacement of camel-casing (used in `jsdoctypeparser` nodes and
    visitor keys is global. The practical effect is that
    `JSDocTypeNamed_parameter` -> `JSDocTypeNamedParameter`,
    `JSDocTypeRecord_entry` -> `JSDocTypeRecordEntry`
    `JSDocTypeNot_nullable` -> `JSDocTypeNotNullable`
    `JSDocTypeInner_member` -> `JSDocTypeInnerMember`
    `JSDocTypeInstance_member` -> `JSDocTypeInstanceMember`
    `JSDocTypeString_value` -> `JSDocTypeStringValue`
    `JSDocTypeNumber_value` -> `JSDocTypeNumberValue`
    `JSDocTypeFile_path` -> `JSDocTypeFilePath`
    `JSDocTypeType_query` -> `JSDocTypeTypeQuery`
    `JSDocTypeKey_query` -> `JSDocTypeKeyQuery`
- Fix: Add missing `JSDocTypeLine` to visitor keys
- Docs: Explain AST structure/differences

## 0.4.1

- Docs: Indicate available methods with brief summary on README

## 0.4.0

- Enhancement: Expose `parseComment` and `getTokenizers`.

## 0.3.0

- Enhancement: Expose `toCamelCase` as new method rather than within a
    utility file.

## 0.2.0

- Enhancement: Exposes new methods: `commentHandler`,
    `commentParserToESTree`, `jsdocVisitorKeys`, `jsdoctypeparserToESTree`,
    `jsdocTypeVisitorKeys`,

## 0.1.1

- Build: Add Babel to work with earlier Node

## 0.1.0

- Initial version
