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
