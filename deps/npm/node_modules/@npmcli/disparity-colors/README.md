# @npmcli/disparity-colors

[![NPM version](https://img.shields.io/npm/v/@npmcli/disparity-colors)](https://www.npmjs.com/package/@npmcli/disparity-colors)
[![Build Status](https://img.shields.io/github/workflow/status/npm/disparity-colors/node-ci)](https://github.com/npm/disparity-colors)
[![License](https://img.shields.io/github/license/npm/disparity-colors)](https://github.com/npm/disparity-colors/blob/master/LICENSE)

Spiritual sucessor to [disparity](https://www.npmjs.com/package/disparity). Colorizes [Diff Unified format](https://en.wikipedia.org/wiki/Diff#Unified_format) output using [ansi-styles](https://www.npmjs.com/package/ansi-styles).

## Install

`npm install @npmcli/disparity-colors`

## Usage:

```js
const colorize = require('@npmcli/disparity-colors')
mapWorkspaces(`--- a/src/index.js
+++ b/src/index.js
@@ -1,4 +1,5 @@
 "use strict";
+"use foo";

 const os = require("os");
`)
// [33m--- a/src/index.js[39m
// [33m+++ b/src/index.js[39m
// [35m@@ -1,4 +1,5 @@[39m
// "use strict";
// [32m+"use foo";[39m
// 
// const os = require("os");
```

## API:

### `colorize(str, opts = {}) -> String`

- `str`: A [Diff Unified format](https://en.wikipedia.org/wiki/Diff#Unified_format) string
- `opts`:
  - `headerLength`: A **Number** defining how many lines should be colorized as header

#### Returns

A **String** including the appropriate [ANSI escape codes](https://en.wikipedia.org/wiki/ANSI_escape_code#Colors_and_Styles)

## LICENSE

[ISC](./LICENSE)

