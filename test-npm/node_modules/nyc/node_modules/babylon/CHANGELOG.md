# Changelog

> **Tags:**
> - [Breaking Change]
> - [Spec Compliancy]
> - [New Feature]
> - [Bug Fix]
> - [Documentation]
> - [Internal]
> - [Polish]

_Note: Gaps between patch versions are faulty, broken or test releases._

See the [Babel Changelog](https://github.com/babel/babel/blob/master/CHANGELOG.md) for the pre-6.8.0 version changelog.

## 6.8.2 (2016-06-24)

### Bug Fixes

- Fix parse error with yielding jsx elements in generators `function* it() { yield <a></a>; }`
- When cloning nodes do not clone its comments
- Fix parse errors when using arrow functions with an spread element and return type `(...props): void => {}`
- Fix leading comments added from previous node
- Fix parse errors with flow's optional arguments `(arg?) => {}`
- Support negative numeric type literals
- Remove line terminator restriction after await keyword
- Remove grouped type arrow restriction as it seems flow no longer has it
- Fix parse error with generic methods that have the name `get` or `set` `class foo { get() {} }`
- Fix parse error with arrow functions that have flow type parameter declarations `<T>(x: T): T => x;`

### Documentation

- Document AST differences from ESTree
- Move ast spec from babel/babel

### Internal

- Enable skipped tests
- Add script to test latest version of babylon with babel
- Upgrade test runner ava
- Add missing generate-identifier-regex script
- Rename parser context types
- Add node v6 to travis testing
- Update to Unicode v9

## 6.8.1 (2016-06-06)

### New Feature

- Parse type parameter declarations with defaults like `type Foo<T = string> = T`

### Bug Fixes
- Type parameter declarations need 1 or more type parameters.
- The existential type `*` is not a valid type parameter.
- The existential type `*` is a primary type

### Spec Compliancy
- The param list for type parameter declarations now consists of `TypeParameter` nodes
- New `TypeParameter` AST Node (replaces using the `Identifier` node before)

```
interface TypeParameter <: Node {
  bound: TypeAnnotation;
  default: TypeAnnotation;
  name: string;
  variance: "plus" | "minus";
}
```

## 6.8.0 (2016-05-02)

#### New Feature

##### Parse Method Parameter Decorators ([#12](https://github.com/babel/babylon/pull/12))

> [Method Parameter Decorators](https://goo.gl/8MmCMG) is now a TC39 [stage 0 proposal](https://github.com/tc39/ecma262/blob/master/stage0.md).

Examples:

```js
class Foo {
  constructor(@foo() x, @bar({ a: 123 }) @baz() y) {}
}

export default function func(@foo() x, @bar({ a: 123 }) @baz() y) {}

var obj = {
  method(@foo() x, @bar({ a: 123 }) @baz() y) {}
};
```

##### Parse for-await statements (w/ `asyncGenerators` plugin) ([#17](https://github.com/babel/babylon/pull/17))

There is also a new node type, `ForAwaitStatement`.

> [Async generators and for-await](https://github.com/tc39/proposal-async-iteration) are now a [stage 2 proposal](https://github.com/tc39/ecma262#current-proposals). 

Example:

```js
async function f() {
  for await (let x of y);
}
```
