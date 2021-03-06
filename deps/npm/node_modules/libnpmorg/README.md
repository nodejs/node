# libnpmorg

[![npm version](https://img.shields.io/npm/v/libnpmorg.svg)](https://npm.im/libnpmorg)
[![license](https://img.shields.io/npm/l/libnpmorg.svg)](https://npm.im/libnpmorg)
[![GitHub Actions](https://github.com/npm/libnpmorg/workflows/Node%20CI/badge.svg)](https://github.com/npm/libnpmorg/workflows/Node%20CI/badge.svg)
[![Coverage Status](https://coveralls.io/repos/github/npm/libnpmorg/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmorg?branch=latest)

[`libnpmorg`](https://github.com/npm/libnpmorg) is a Node.js library for
programmatically accessing the [npm Org membership
API](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#membership-detail).

## Table of Contents

* [Example](#example)
* [Install](#install)
* [Contributing](#contributing)
* [API](#api)
  * [hook opts](#opts)
  * [`set()`](#set)
  * [`rm()`](#rm)
  * [`ls()`](#ls)
  * [`ls.stream()`](#ls-stream)

## Example

```js
const org = require('libnpmorg')

console.log(await org.ls('myorg', {token: 'deadbeef'}))
=>
Roster {
  zkat: 'developer',
  iarna: 'admin',
  isaacs: 'owner'
}
```

## Install

`$ npm install libnpmorg`

### API

#### <a name="opts"></a> `opts` for `libnpmorg` commands

`libnpmorg` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).
All options are passed through directly to that library, so please refer to [its
own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

A couple of options of note for those in a hurry:

* `opts.token` - can be passed in and will be used as the authentication token for the registry. For other ways to pass in auth details, see the n-r-f docs.
* `opts.otp` - certain operations will require an OTP token to be passed in. If a `libnpmorg` command fails with `err.code === EOTP`, please retry the request with `{otp: <2fa token>}`

#### <a name="set"></a> `> org.set(org, user, [role], [opts]) -> Promise`

The returned Promise resolves to a [Membership
Detail](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#membership-detail)
object.

The `role` is optional and should be one of `admin`, `owner`, or `developer`.
`developer` is the default if no `role` is provided.

`org` and `user` must be scope names for the org name and user name
respectively. They can optionally be prefixed with `@`.

See also: [`PUT
/-/org/:scope/user`](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#org-membership-replace)

##### Example

```javascript
await org.set('@myorg', '@myuser', 'admin', {token: 'deadbeef'})
=>
MembershipDetail {
  org: {
    name: 'myorg',
    size: 15
  },
  user: 'myuser',
  role: 'admin'
}
```

#### <a name="rm"></a> `> org.rm(org, user, [opts]) -> Promise`

The Promise resolves to `null` on success.

`org` and `user` must be scope names for the org name and user name
respectively. They can optionally be prefixed with `@`.

See also: [`DELETE
/-/org/:scope/user`](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#org-membership-delete)

##### Example

```javascript
await org.rm('myorg', 'myuser', {token: 'deadbeef'})
```

#### <a name="ls"></a> `> org.ls(org, [opts]) -> Promise`

The Promise resolves to a
[Roster](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#roster)
object.

`org` must be a scope name for an org, and can be optionally prefixed with `@`.

See also: [`GET
/-/org/:scope/user`](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#org-roster)

##### Example

```javascript
await org.ls('myorg', {token: 'deadbeef'})
=>
Roster {
  zkat: 'developer',
  iarna: 'admin',
  isaacs: 'owner'
}
```

#### <a name="ls-stream"></a> `> org.ls.stream(org, [opts]) -> Stream`

Returns a stream of entries for a
[Roster](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#roster),
with each emitted entry in `[key, value]` format.

`org` must be a scope name for an org, and can be optionally prefixed with `@`.

The returned stream is a valid `Symbol.asyncIterator`.

See also: [`GET
/-/org/:scope/user`](https://github.com/npm/registry/blob/master/docs/orgs/memberships.md#org-roster)

##### Example

```javascript
for await (let [user, role] of org.ls.stream('myorg', {token: 'deadbeef'})) {
  console.log(`user: ${user} (${role})`)
}
=>
user: zkat (developer)
user: iarna (admin)
user: isaacs (owner)
```
