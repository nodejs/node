# @babel/code-frame

> Generate errors that contain a code frame that point to source locations.

## Install

```sh
npm install --save-dev @babel/code-frame
```

## Usage

```js
import { codeFrameColumns } from '@babel/code-frame';

const rawLines = `class Foo {
  constructor()
}`;
const location = { start: { line: 2, column: 16 } };

const result = codeFrameColumns(rawLines, location, { /* options */ });

console.log(result);
```

```
  1 | class Foo {
> 2 |   constructor()
    |                ^
  3 | }
```

If the column number is not known, you may omit it.

You can also pass an `end` hash in `location`.

```js
import { codeFrameColumns } from '@babel/code-frame';

const rawLines = `class Foo {
  constructor() {
    console.log("hello");
  }
}`;
const location = { start: { line: 2, column: 17 }, end: { line: 4, column: 3 } };

const result = codeFrameColumns(rawLines, location, { /* options */ });

console.log(result);
```

```
  1 | class Foo {
> 2 |   constructor() {
    |                 ^
> 3 |     console.log("hello");
    | ^^^^^^^^^^^^^^^^^^^^^^^^^
> 4 |   }
    | ^^^
  5 | };
```

## Options

### `highlightCode`

`boolean`, defaults to `false`.

Toggles syntax highlighting the code as JavaScript for terminals.

### `linesAbove`

`number`, defaults to `2`.

Adjust the number of lines to show above the error.

### `linesBelow`

`number`, defaults to `3`.

Adjust the number of lines to show below the error.

### `forceColor`

`boolean`, defaults to `false`.

Enable this to forcibly syntax highlight the code as JavaScript (for non-terminals); overrides `highlightCode`.

## Upgrading from prior versions

Prior to version 7, the only API exposed by this module was for a single line and optional column pointer. The old API will now log a deprecation warning.

The new API takes a `location` object, similar to what is available in an AST.

This is an example of the deprecated (but still available) API:

```js
import codeFrame from '@babel/code-frame';

const rawLines = `class Foo {
  constructor()
}`;
const lineNumber = 2;
const colNumber = 16;

const result = codeFrame(rawLines, lineNumber, colNumber, { /* options */ });

console.log(result);
```

To get the same highlighting using the new API:

```js
import { codeFrameColumns } from '@babel/code-frame';

const rawLines = `class Foo {
  constructor() {
    console.log("hello");
  }
}`;
const location = { start: { line: 2, column: 16 } };

const result = codeFrameColumns(rawLines, location, { /* options */ });

console.log(result);
```
