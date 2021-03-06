# libnpmteam

[![npm version](https://img.shields.io/npm/v/libnpmteam.svg)](https://npm.im/libnpmteam)
[![license](https://img.shields.io/npm/l/libnpmteam.svg)](https://npm.im/libnpmteam)
[![GitHub Actions](https://github.com/npm/libnpmteam/workflows/Node%20CI/badge.svg)](https://github.com/npm/libnpmteam/workflows/Node%20CI/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/npm/libnpmteam/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmteam?branch=latest)

[`libnpmteam`](https://github.com/npm/libnpmteam) is a Node.js
library that provides programmatic access to the guts of the npm CLI's `npm
team` command and its various subcommands.

## Example

```javascript
const access = require('libnpmteam')

// List all teams for the @npm org.
console.log(await team.lsTeams('npm'))
```

## Publishing
1. Manually create CHANGELOG.md file
1. Commit changes to CHANGELOG.md
    ```bash
    $ git commit -m "chore: updated CHANGELOG.md"
    ```
1. Run `npm version {newVersion}`
    ```bash
    # Example
    $ npm version patch
    # 1. Runs `coverage` and `lint` scripts
    # 2. Bumps package version; and **create commit/tag**
    # 3. Runs `npm publish`; publishing directory with **unpushed commit**
    # 4. Runs `git push origin --follow-tags`
    ```

## Table of Contents

* [Installing](#install)
* [Example](#example)
* [API](#api)
  * [team opts](#opts)
  * [`create()`](#create)
  * [`destroy()`](#destroy)
  * [`add()`](#add)
  * [`rm()`](#rm)
  * [`lsTeams()`](#ls-teams)
  * [`lsTeams.stream()`](#ls-teams-stream)
  * [`lsUsers()`](#ls-users)
  * [`lsUsers.stream()`](#ls-users-stream)

### Install

`$ npm install libnpmteam`

### API

#### <a name="opts"></a> `opts` for `libnpmteam` commands

`libnpmteam` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).
All options are passed through directly to that library, so please refer to [its
own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

A couple of options of note for those in a hurry:

* `opts.token` - can be passed in and will be used as the authentication token for the registry. For other ways to pass in auth details, see the n-r-f docs.
* `opts.otp` - certain operations will require an OTP token to be passed in. If a `libnpmteam` command fails with `err.code === EOTP`, please retry the request with `{otp: <2fa token>}`

#### <a name="create"></a> `> team.create(team, [opts]) -> Promise`

Creates a team named `team`. Team names use the format `@<scope>:<name>`, with
the `@` being optional.

Additionally, `opts.description` may be passed in to include a description.

##### Example

```javascript
await team.create('@npm:cli', {token: 'myregistrytoken'})
// The @npm:cli team now exists.
```

#### <a name="destroy"></a> `> team.destroy(team, [opts]) -> Promise`

Destroys a team named `team`. Team names use the format `@<scope>:<name>`, with
the `@` being optional.

##### Example

```javascript
await team.destroy('@npm:cli', {token: 'myregistrytoken'})
// The @npm:cli team has been destroyed.
```

#### <a name="add"></a> `> team.add(user, team, [opts]) -> Promise`

Adds `user` to `team`.

##### Example

```javascript
await team.add('zkat', '@npm:cli', {token: 'myregistrytoken'})
// @zkat now belongs to the @npm:cli team.
```

#### <a name="rm"></a> `> team.rm(user, team, [opts]) -> Promise`

Removes `user` from `team`.

##### Example

```javascript
await team.rm('zkat', '@npm:cli', {token: 'myregistrytoken'})
// @zkat is no longer part of the @npm:cli team.
```

#### <a name="ls-teams"></a> `> team.lsTeams(scope, [opts]) -> Promise`

Resolves to an array of team names belonging to `scope`.

##### Example

```javascript
await team.lsTeams('@npm', {token: 'myregistrytoken'})
=>
[
  'npm:cli',
  'npm:web',
  'npm:registry',
  'npm:developers'
]
```

#### <a name="ls-teams-stream"></a> `> team.lsTeams.stream(scope, [opts]) -> Stream`

Returns a stream of teams belonging to `scope`.

For a Promise-based version of these results, see [`team.lsTeams()`](#ls-teams).

##### Example

```javascript
for await (let team of team.lsTeams.stream('@npm', {token: 'myregistrytoken'})) {
  console.log(team)
}

// outputs
// npm:cli
// npm:web
// npm:registry
// npm:developers
```

#### <a name="ls-users"></a> `> team.lsUsers(team, [opts]) -> Promise`

Resolves to an array of usernames belonging to `team`.

For a streamed version of these results, see [`team.lsUsers.stream()`](#ls-users-stream).

##### Example

```javascript
await team.lsUsers('@npm:cli', {token: 'myregistrytoken'})
=>
[
  'iarna',
  'zkat'
]
```

#### <a name="ls-users-stream"></a> `> team.lsUsers.stream(team, [opts]) -> Stream`

Returns a stream of usernames belonging to `team`.

For a Promise-based version of these results, see [`team.lsUsers()`](#ls-users).

##### Example

```javascript
for await (let user of team.lsUsers.stream('@npm:cli', {token: 'myregistrytoken'})) {
  console.log(user)
}

// outputs
// iarna
// zkat
```
