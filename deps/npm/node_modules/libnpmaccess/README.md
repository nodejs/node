# libnpmaccess

[![npm version](https://img.shields.io/npm/v/libnpmaccess.svg)](https://npm.im/libnpmaccess)
[![license](https://img.shields.io/npm/l/libnpmaccess.svg)](https://npm.im/libnpmaccess)
[![GitHub Actions](https://github.com/npm/libnpmaccess/workflows/Node%20CI/badge.svg)](https://github.com/npm/libnpmaccess/actions?query=workflow%3A%22Node+CI%22)
[![Coverage Status](https://coveralls.io/repos/github/npm/libnpmaccess/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmaccess?branch=latest)

[`libnpmaccess`](https://github.com/npm/libnpmaccess) is a Node.js
library that provides programmatic access to the guts of the npm CLI's `npm
access` command and its various subcommands. This includes managing account 2FA,
listing packages and permissions, looking at package collaborators, and defining
package permissions for users, orgs, and teams.

## Example

```javascript
const access = require('libnpmaccess')

// List all packages @zkat has access to on the npm registry.
console.log(Object.keys(await access.lsPackages('zkat')))
```

## Table of Contents

* [Installing](#install)
* [Example](#example)
* [Contributing](#contributing)
* [API](#api)
  * [access opts](#opts)
  * [`public()`](#public)
  * [`restricted()`](#restricted)
  * [`grant()`](#grant)
  * [`revoke()`](#revoke)
  * [`tfaRequired()`](#tfa-required)
  * [`tfaNotRequired()`](#tfa-not-required)
  * [`lsPackages()`](#ls-packages)
  * [`lsPackages.stream()`](#ls-packages-stream)
  * [`lsCollaborators()`](#ls-collaborators)
  * [`lsCollaborators.stream()`](#ls-collaborators-stream)

### Install

`$ npm install libnpmaccess`

### API

#### <a name="opts"></a> `opts` for `libnpmaccess` commands

`libnpmaccess` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).
All options are passed through directly to that library, so please refer to [its
own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

A couple of options of note for those in a hurry:

* `opts.token` - can be passed in and will be used as the authentication token for the registry. For other ways to pass in auth details, see the n-r-f docs.
* `opts.otp` - certain operations will require an OTP token to be passed in. If a `libnpmaccess` command fails with `err.code === EOTP`, please retry the request with `{otp: <2fa token>}`

#### <a name="public"></a> `> access.public(spec, [opts]) -> Promise<Boolean>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec.

Makes package described by `spec` public.

##### Example

```javascript
await access.public('@foo/bar', {token: 'myregistrytoken'})
// `@foo/bar` is now public
```

#### <a name="restricted"></a> `> access.restricted(spec, [opts]) -> Promise<Boolean>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec.

Makes package described by `spec` private/restricted.

##### Example

```javascript
await access.restricted('@foo/bar', {token: 'myregistrytoken'})
// `@foo/bar` is now private
```

#### <a name="grant"></a> `> access.grant(spec, team, permissions, [opts]) -> Promise<Boolean>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec. `team` must be a fully-qualified team name, in the `scope:team`
format, with or without the `@` prefix, and the team must be a valid team within
that scope. `permissions` must be one of `'read-only'` or `'read-write'`.

Grants `read-only` or `read-write` permissions for a certain package to a team.

##### Example

```javascript
await access.grant('@foo/bar', '@foo:myteam', 'read-write', {
  token: 'myregistrytoken'
})
// `@foo/bar` is now read/write enabled for the @foo:myteam team.
```

#### <a name="revoke"></a> `> access.revoke(spec, team, [opts]) -> Promise<Boolean>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec. `team` must be a fully-qualified team name, in the `scope:team`
format, with or without the `@` prefix, and the team must be a valid team within
that scope. `permissions` must be one of `'read-only'` or `'read-write'`.

Removes access to a package from a certain team.

##### Example

```javascript
await access.revoke('@foo/bar', '@foo:myteam', {
  token: 'myregistrytoken'
})
// @foo:myteam can no longer access `@foo/bar`
```

#### <a name="tfa-required"></a> `> access.tfaRequired(spec, [opts]) -> Promise<Boolean>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec.

Makes it so publishing or managing a package requires using 2FA tokens to
complete operations.

##### Example

```javascript
await access.tfaRequires('lodash', {token: 'myregistrytoken'})
// Publishing or changing dist-tags on `lodash` now require OTP to be enabled.
```

#### <a name="tfa-not-required"></a> `> access.tfaNotRequired(spec, [opts]) -> Promise<Boolean>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec.

Disabled the package-level 2FA requirement for `spec`. Note that you will need
to pass in an `otp` token in `opts` in order to complete this operation.

##### Example

```javascript
await access.tfaNotRequired('lodash', {otp: '123654', token: 'myregistrytoken'})
// Publishing or editing dist-tags on `lodash` no longer requires OTP to be
// enabled.
```

#### <a name="ls-packages"></a> `> access.lsPackages(entity, [opts]) -> Promise<Object | null>`

`entity` must be either a valid org or user name, or a fully-qualified team name
in the `scope:team` format, with or without the `@` prefix.

Lists out packages a user, org, or team has access to, with corresponding
permissions. Packages that the access token does not have access to won't be
listed.

In order to disambiguate between users and orgs, two requests may end up being
made when listing orgs or users.

For a streamed version of these results, see
[`access.lsPackages.stream()`](#ls-package-stream).

##### Example

```javascript
await access.lsPackages('zkat', {
  token: 'myregistrytoken'
})
// Lists all packages `@zkat` has access to on the registry, and the
// corresponding permissions.
```

#### <a name="ls-packages-stream"></a> `> access.lsPackages.stream(scope, [team], [opts]) -> Stream`

`entity` must be either a valid org or user name, or a fully-qualified team name
in the `scope:team` format, with or without the `@` prefix.

Streams out packages a user, org, or team has access to, with corresponding
permissions, with each stream entry being formatted like `[packageName,
permissions]`. Packages that the access token does not have access to won't be
listed.

In order to disambiguate between users and orgs, two requests may end up being
made when listing orgs or users.

The returned stream is a valid `asyncIterator`.

##### Example

```javascript
for await (let [pkg, perm] of access.lsPackages.stream('zkat')) {
  console.log('zkat has', perm, 'access to', pkg)
}
// zkat has read-write access to eggplant
// zkat has read-only access to @npmcorp/secret
```

#### <a name="ls-collaborators"></a> `> access.lsCollaborators(spec, [user], [opts]) -> Promise<Object | null>`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec. `user` must be a valid user name, with or without the `@`
prefix.

Lists out access privileges for a certain package. Will only show permissions
for packages to which you have at least read access. If `user` is passed in, the
list is filtered only to teams _that_ user happens to belong to.

For a streamed version of these results, see [`access.lsCollaborators.stream()`](#ls-collaborators-stream).

##### Example

```javascript
await access.lsCollaborators('@npm/foo', 'zkat', {
  token: 'myregistrytoken'
})
// Lists all teams with access to @npm/foo that @zkat belongs to.
```

#### <a name="ls-collaborators-stream"></a> `> access.lsCollaborators.stream(spec, [user], [opts]) -> Stream`

`spec` must be an [`npm-package-arg`](https://npm.im/npm-package-arg)-compatible
registry spec. `user` must be a valid user name, with or without the `@`
prefix.

Stream out access privileges for a certain package, with each entry in `[user,
permissions]` format. Will only show permissions for packages to which you have
at least read access. If `user` is passed in, the list is filtered only to teams
_that_ user happens to belong to.

The returned stream is a valid `asyncIterator`.

##### Example

```javascript
for await (let [usr, perm] of access.lsCollaborators.stream('npm')) {
  console.log(usr, 'has', perm, 'access to npm')
}
// zkat has read-write access to npm
// iarna has read-write access to npm
```
