# @npmcli/map-workspaces

[![NPM version](https://img.shields.io/npm/v/@npmcli/map-workspaces)](https://www.npmjs.com/package/@npmcli/map-workspaces)
[![Build Status](https://img.shields.io/github/workflow/status/npm/map-workspaces/node-ci)](https://github.com/npm/map-workspaces)
[![License](https://img.shields.io/github/license/npm/map-workspaces)](https://github.com/npm/map-workspaces/blob/master/LICENSE)

Retrieves a name:pathname Map for a given workspaces config.

Long version: Reads the `workspaces` property from a valid **workspaces configuration** object and traverses the paths and globs defined there in order to find valid nested packages and return a **Map** of all found packages where keys are package names and values are folder locations.

## Install

`npm install map-workspaces`

## Usage:

```js
const mapWorkspaces = require('@npmcli/map-workspaces')
await mapWorkspaces({
  cwd,
  pkg: {
    workspaces: {
      packages: [
        "a",
        "b"
      ]
    }
  }
})
// ->
// Map {
//   'a': '<cwd>/a'
//   'b': '<cwd>/b'
// }
```

## Examples:

### Glob usage:

Given a folder structure such as:

```
├── package.json
└── apps
   ├── a
   │   └── package.json
   ├── b
   │   └── package.json
   └── c
       └── package.json
```

```js
const mapWorkspaces = require('@npmcli/map-workspaces')
await mapWorkspaces({
  cwd,
  pkg: {
    workspaces: [
      "apps/*"
    ]
  }
})
// ->
// Map {
//   'a': '<cwd>/apps/a'
//   'b': '<cwd>/apps/b'
//   'c': '<cwd>/apps/c'
// }
```

## API:

### `mapWorkspaces(opts) -> Promise<Map>`

- `opts`:
  - `pkg`: A valid `package.json` **Object**
  - `cwd`: A **String** defining the base directory to use when reading globs and paths.
  - `ignore`: An **Array** of paths to be ignored when using [globs](https://www.npmjs.com/package/glob) to look for nested package.
  - ...[Also support all other glob options](https://www.npmjs.com/package/glob#options)

#### Returns

A **Map** in which keys are **package names** and values are the **pathnames** for each found **workspace**.

## LICENSE

[ISC](./LICENSE)

