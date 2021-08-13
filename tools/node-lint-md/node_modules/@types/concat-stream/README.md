# Installation
> `npm install --save @types/concat-stream`

# Summary
This package contains type definitions for concat-stream (https://github.com/maxogden/concat-stream).

# Details
Files were exported from https://github.com/DefinitelyTyped/DefinitelyTyped/tree/master/types/concat-stream.
## [index.d.ts](https://github.com/DefinitelyTyped/DefinitelyTyped/tree/master/types/concat-stream/index.d.ts)
````ts
// Type definitions for concat-stream 1.6
// Project: https://github.com/maxogden/concat-stream
// Definitions by: Joey Marianer <https://github.com/jmarianer>
// Definitions: https://github.com/DefinitelyTyped/DefinitelyTyped

/// <reference types="node" />

import { Writable } from "stream";

interface ConcatOpts {
  encoding?: string | undefined;
}

declare function concat(cb: (buf: Buffer) => void): Writable;
declare function concat(opts: ConcatOpts, cb: (buf: Buffer) => void): Writable;

export = concat;

````

### Additional Details
 * Last updated: Tue, 06 Jul 2021 18:05:59 GMT
 * Dependencies: [@types/node](https://npmjs.com/package/@types/node)
 * Global values: none

# Credits
These definitions were written by [Joey Marianer](https://github.com/jmarianer).
