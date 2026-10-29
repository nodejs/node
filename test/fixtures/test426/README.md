# Source Map Tests

This repository holds discussions on testing and tests for the Source Map debugging format. Specifically, we're looking to encourage discussion around:

- Manual and automated testing strategies for Source Maps
- Gathering a list of Source Map generators and consumers
- General discussion around deviations between source maps

Open discussion happens in the [GitHub issues](https://github.com/source-map/source-map-tests/issues).

Source Map spec:
  * Repo: https://github.com/tc39/source-map
  * Rendered spec: https://tc39.es/source-map/

## Test cases

This repo also contains cross-implementation test cases that can be run in test
suites for source map implementations, including browser devtool and library test
suites.

### Running the tests

#### Tools

[Source map validator](https://github.com/jkup/source-map-validator):
  * The tests are included in the validator test suite [here](https://github.com/jkup/source-map-validator/blob/main/src/spec-tests.test.ts). You can run them with `npm test`.

#### Browsers

The tests for Firefox are in the Mozilla [source-map](https://github.com/mozilla/source-map) library:
  * The upstream repo has a [test file](https://github.com/mozilla/source-map/blob/master/test/test-spec-tests.js) for running the spec tests from this repo. They can be run with `npm test`.

How to run in WebKit:
  * Check out [WebKit](https://github.com/WebKit/WebKit/)
  * `cd` to the checked out WebKit directory.
  * Run `git am <this-repo>/webkit/0001-Add-harness-for-source-maps-spec-tests.patch`
  * Run `Tools/Scripts/build-webkit` (depending on the platform you may need to pass `--gtk` or other flags)
  * Run `Tools/Scripts/run-webkit-tests LayoutTests/inspector/model/source-map-spec.html` (again, you may need `--gtk` on Linux)

How to run in Chrome Devtools:
1. Setup:
    * Install depot_tools following this [depot_tools guide](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up)
    * Check out [Chrome Devtools](https://chromium.googlesource.com/devtools/devtools-frontend):
    * Run `gclient config https://chromium.googlesource.com/devtools/devtools-frontend --unmanaged`
    * Run `cd devtools-frontend`
    * Run `gclient sync`
    * Run `gn gen out/Default`
2. Build:
    * Run `autoninja -C out/Default`
3. Test:
    * Run `npm run auto-unittest`
4. Apply patches from this repo:
    * Run `git apply <path to .patch file>` in `devtools-frontend` repo

More information about running Chrome Devtools without building Chromium can be found [here](https://chromium.googlesource.com/devtools/devtools-frontend/+/refs/heads/chromium/3965/README.md)

#### Node.js

Follow the [running Node.js tests guide](https://github.com/nodejs/node/blob/main/BUILDING.md#running-tests) to build Node.js and run tests locally.
Read more about running the source map tests at [here](https://github.com/nodejs/node/tree/main/test/test426).

## npm package

The test cases are published to npm as [`@tc39/source-map-tests`](https://www.npmjs.com/package/@tc39/source-map-tests), so that source map implementations can run them in their own test suites. A new version is published for every commit to `main`.

```sh
npm install --save-dev --save-exact @tc39/source-map-tests
```

### Usage

`@tc39/source-map-tests/suites.json` lists the test suites:

```json
{
  "suites": [
    { "kind": "interpreting", "file": "interpreting/source-map-spec.json" },
    { "kind": "decoding", "file": "decoding/scopes.json" }
  ]
}
```

There are two types of test suites: `interpreting` and `decoding`:
- **interpreting** provided a source map to apply to a generated file, together with some actions to perform through it to check that it has been applied correctly.
- **decoding** suites check that a source map is correctly decoded into a [Decoded Source Map Record](https://tc39.es/ecma426/#decoded-source-map-record).

### TypeScript

The package's root exports the types of the JSON files:

```ts
import type { InterpretingSuite, DecodingSuite, TestAction } from "@tc39/source-map-tests";
```

With TypeScript 7.1 or later, you can load type definitions for the various suites by adding `"types": ["@tc39/source-map-tests"]` to your `tsconfig.json`. Imported JSON modules then will be typed appropriately:
```ts
import spec from "@tc39/source-map-tests/interpreting/source-map-spec.json" with { type: "json" };
spec.tests[0].testActions; // TestAction[]
```
