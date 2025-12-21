# Decoding tests

This directory contains various source map files along with expectations to help implementations verify source map decoding.

Each source map `<file name>.map`, is accompanied by:

  * a `<file name>.map.golden` file, containing a [Decoded Source Map Record](https://tc39.es/ecma426/#decoded-source-map-record) in JSON format
  * an optional `<file name>.map.error` file. It is present for source maps where the spec would ["optionally report an error"](https://tc39.es/ecma426/#sec-optional-errors). At the moment, the `<file name>.error` file only serves as a marker. But in the future it may be turned into a JSON with more information about the number and specifics of the expected errors.

## .golden format

The JSON format of the `.golden` files, attempts to follow the [Decoded Source Map Record](https://tc39.es/ecma426/#decoded-source-map-record) closely, but since the "Decoded Source Map Record" has cycles, we deviate slightly. The exact format is documented in [./source-map-record.d.ts](./source-map-record.d.ts).

## Proposals

Each source map proposal gets its own sub-directory. If a proposal adds new specification records or extends existing records, the sub-directory should contain a separate `<proposal>/<proposal>.d.ts` that describes the extended golden files. See [./scopes/scopes.d.ts](./scopes/scopes.d.ts) as an example. Once a proposal reaches stage 4, the `<proposal>/<proposal>.d.ts` file is merged into the [./source-map-record.d.ts](./source-map-record.d.ts).
