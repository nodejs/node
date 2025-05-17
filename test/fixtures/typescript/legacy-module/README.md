# Legacy TypeScript Module

When `tsconfig.json` is set to `module: "node16"` or any `node*`, the TypeScript compiler will
produce the output in the format by the extension (e.g. `.cts` or `.mts`), or set by the
`package.json#type` field, regardless of the syntax of the original source code.
