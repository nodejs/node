# libnpmexec

[![npm version](https://img.shields.io/npm/v/libnpmexec.svg)](https://npm.im/libnpmexec)
[![license](https://img.shields.io/npm/l/libnpmexec.svg)](https://npm.im/libnpmexec)
[![GitHub Actions](https://github.com/npm/libnpmexec/workflows/node-ci/badge.svg)](https://github.com/npm/libnpmexec/actions?query=workflow%3Anode-ci)
[![Coverage Status](https://coveralls.io/repos/github/npm/libnpmexec/badge.svg?branch=main)](https://coveralls.io/github/npm/libnpmexec?branch=main)

The `npm exec` (`npx`) Programmatic API

## Install

`npm install libnpmexec`

## Usage:

```js
const libexec = require('libnpmexec')
await libexec({
  args: ['yosay', 'Bom dia!'],
  cache: '~/.npm',
  yes: true,
})
```

## API:

### `libexec(opts)`

- `opts`:
  - `args`: List of pkgs to execute **Array<String>**, defaults to `[]`
  - `call`: An alternative command to run when using `packages` option **String**, defaults to empty string.
  - `cache`: The path location to where the npm cache folder is placed **String**
  - `color`: Output should use color? **Boolean**, defaults to `false`
  - `localBin`: Location to the `node_modules/.bin` folder of the local project **String**, defaults to empty string.
  - `locationMsg`: Overrides "at location" message when entering interactive mode **String**
  - `log`: Sets an optional logger **Object**, defaults to `proc-log` module usage.
  - `globalBin`: Location to the global space bin folder, same as: `$(npm bin -g)` **String**, defaults to empty string.
  - `output`: A function to print output to **Function**
  - `packages`: A list of packages to be used (possibly fetch from the registry) **Array<String>**, defaults to `[]`
  - `path`: Location to where to read local project info (`package.json`) **String**, defaults to `.`
  - `runPath`: Location to where to execute the script **String**, defaults to `.`
  - `scriptShell`: Default shell to be used **String**
  - `yes`: Should skip download confirmation prompt when fetching missing packages from the registry? **Boolean**
  - `registry`, `cache`, and more options that are forwarded to [@npmcli/arborist](https://github.com/npm/arborist/) and [pacote](https://github.com/npm/pacote/#options) **Object**

## LICENSE

[ISC](./LICENSE)
