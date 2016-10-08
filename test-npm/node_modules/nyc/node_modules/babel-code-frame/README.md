# babel-code-frame

> Generate errors that contain a code frame that point to source locations.

## Install

```sh
$ npm install babel-code-frame
```

## Usage

```js
import codeFrame from 'babel-code-frame';

const rawLines = `class Foo {
  constructor()
}`;
const lineNumber = 2;
const colNumber = 16;

const result = codeFrame(rawLines, lineNumber, colNumber, { /* options */ });

console.log(result);
```

```sh
  1 | class Foo {
> 2 |   constructor()
    |                ^
  3 | }
```

If the column number is not known, you may pass `null` instead.

## Options

name                   | type     | default         | description
-----------------------|----------|-----------------|------------------------------------------------------
highlightCode          | boolean  | `false`         | Syntax highlight the code as JavaScript for terminals
