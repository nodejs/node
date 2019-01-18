# libnpmhook [![npm version](https://img.shields.io/npm/v/libnpmhook.svg)](https://npm.im/libnpmhook) [![license](https://img.shields.io/npm/l/libnpmhook.svg)](https://npm.im/libnpmhook) [![Travis](https://img.shields.io/travis/npm/libnpmhook.svg)](https://travis-ci.org/npm/libnpmhook) [![AppVeyor](https://ci.appveyor.com/api/projects/status/github/zkat/libnpmhook?svg=true)](https://ci.appveyor.com/project/zkat/libnpmhook) [![Coverage Status](https://coveralls.io/repos/github/npm/libnpmhook/badge.svg?branch=latest)](https://coveralls.io/github/npm/libnpmhook?branch=latest)

[`libnpmhook`](https://github.com/npm/libnpmhook) is a Node.js library for
programmatically managing the npm registry's server-side hooks.

For a more general introduction to managing hooks, see [the introductory blog
post](https://blog.npmjs.org/post/145260155635/introducing-hooks-get-notifications-of-npm).

## Example

```js
const hooks = require('libnpmhook')

console.log(await hooks.ls('mypkg', {token: 'deadbeef'}))
// array of hook objects on `mypkg`.
```

## Install

`$ npm install libnpmhook`

## Table of Contents

* [Example](#example)
* [Install](#install)
* [API](#api)
  * [hook opts](#opts)
  * [`add()`](#add)
  * [`rm()`](#rm)
  * [`ls()`](#ls)
  * [`ls.stream()`](#ls-stream)
  * [`update()`](#update)

### API

#### <a name="opts"></a> `opts` for `libnpmhook` commands

`libnpmhook` uses [`npm-registry-fetch`](https://npm.im/npm-registry-fetch).
All options are passed through directly to that library, so please refer to [its
own `opts`
documentation](https://www.npmjs.com/package/npm-registry-fetch#fetch-options)
for options that can be passed in.

A couple of options of note for those in a hurry:

* `opts.token` - can be passed in and will be used as the authentication token for the registry. For other ways to pass in auth details, see the n-r-f docs.
* `opts.otp` - certain operations will require an OTP token to be passed in. If a `libnpmhook` command fails with `err.code === EOTP`, please retry the request with `{otp: <2fa token>}`
* `opts.Promise` - If you pass this in, the Promises returned by `libnpmhook` commands will use this Promise class instead. For example: `{Promise: require('bluebird')}`

#### <a name="add"></a> `> hooks.add(name, endpoint, secret, [opts]) -> Promise`

`name` is the name of the package, org, or user/org scope to watch. The type is
determined by the name syntax: `'@foo/bar'` and `'foo'` are treated as packages,
`@foo` is treated as a scope, and `~user` is treated as an org name or scope.
Each type will attach to different events.

The `endpoint` should be a fully-qualified http URL for the endpoint the hook
will send its payload to when it fires. `secret` is a shared secret that the
hook will send to that endpoint to verify that it's actually coming from the
registry hook.

The returned Promise resolves to the full hook object that was created,
including its generated `id`.

See also: [`POST
/v1/hooks/hook`](https://github.com/npm/registry/blob/master/docs/hooks/endpoints.md#post-v1hookshook)

##### Example

```javascript
await hooks.add('~zkat', 'https://zkat.tech/api/added', 'supersekrit', {
  token: 'myregistrytoken',
  otp: '694207'
})

=>

{ id: '16f7xoal',
  username: 'zkat',
  name: 'zkat',
  endpoint: 'https://zkat.tech/api/added',
  secret: 'supersekrit',
  type: 'owner',
  created: '2018-08-21T20:05:25.125Z',
  updated: '2018-08-21T20:05:25.125Z',
  deleted: false,
  delivered: false,
  last_delivery: null,
  response_code: 0,
  status: 'active' }
```

#### <a name="find"></a> `> hooks.find(id, [opts]) -> Promise`

Returns the hook identified by `id`.

The returned Promise resolves to the full hook object that was found, or error
with `err.code` of `'E404'` if it didn't exist.

See also: [`GET
/v1/hooks/hook/:id`](https://github.com/npm/registry/blob/master/docs/hooks/endpoints.md#get-v1hookshookid)

##### Example

```javascript
await hooks.find('16f7xoal', {token: 'myregistrytoken'})

=>

{ id: '16f7xoal',
  username: 'zkat',
  name: 'zkat',
  endpoint: 'https://zkat.tech/api/added',
  secret: 'supersekrit',
  type: 'owner',
  created: '2018-08-21T20:05:25.125Z',
  updated: '2018-08-21T20:05:25.125Z',
  deleted: false,
  delivered: false,
  last_delivery: null,
  response_code: 0,
  status: 'active' }
```

#### <a name="rm"></a> `> hooks.rm(id, [opts]) -> Promise`

Removes the hook identified by `id`.

The returned Promise resolves to the full hook object that was removed, if it
existed, or `null` if no such hook was there (instead of erroring).

See also: [`DELETE
/v1/hooks/hook/:id`](https://github.com/npm/registry/blob/master/docs/hooks/endpoints.md#delete-v1hookshookid)

##### Example

```javascript
await hooks.rm('16f7xoal', {
  token: 'myregistrytoken',
  otp: '694207'
})

=>

{ id: '16f7xoal',
  username: 'zkat',
  name: 'zkat',
  endpoint: 'https://zkat.tech/api/added',
  secret: 'supersekrit',
  type: 'owner',
  created: '2018-08-21T20:05:25.125Z',
  updated: '2018-08-21T20:05:25.125Z',
  deleted: true,
  delivered: false,
  last_delivery: null,
  response_code: 0,
  status: 'active' }

// Repeat it...
await hooks.rm('16f7xoal', {
  token: 'myregistrytoken',
  otp: '694207'
})

=> null
```

#### <a name="update"></a> `> hooks.update(id, endpoint, secret, [opts]) -> Promise`

The `id` should be a hook ID from a previously-created hook.

The `endpoint` should be a fully-qualified http URL for the endpoint the hook
will send its payload to when it fires. `secret` is a shared secret that the
hook will send to that endpoint to verify that it's actually coming from the
registry hook.

The returned Promise resolves to the full hook object that was updated, if it
existed. Otherwise, it will error with an `'E404'` error code.

See also: [`PUT
/v1/hooks/hook/:id`](https://github.com/npm/registry/blob/master/docs/hooks/endpoints.md#put-v1hookshookid)

##### Example

```javascript
await hooks.update('16fxoal', 'https://zkat.tech/api/other', 'newsekrit', {
  token: 'myregistrytoken',
  otp: '694207'
})

=>

{ id: '16f7xoal',
  username: 'zkat',
  name: 'zkat',
  endpoint: 'https://zkat.tech/api/other',
  secret: 'newsekrit',
  type: 'owner',
  created: '2018-08-21T20:05:25.125Z',
  updated: '2018-08-21T20:14:41.964Z',
  deleted: false,
  delivered: false,
  last_delivery: null,
  response_code: 0,
  status: 'active' }
```

#### <a name="ls"></a> `> hooks.ls([opts]) -> Promise`

Resolves to an array of hook objects associated with the account you're
authenticated as.

Results can be further filtered with three values that can be passed in through
`opts`:

* `opts.package` - filter results by package name
* `opts.limit` - maximum number of hooks to return
* `opts.offset` - pagination offset for results (use with `opts.limit`)

See also:
  * [`hooks.ls.stream()`](#ls-stream)
  * [`GET
/v1/hooks`](https://github.com/npm/registry/blob/master/docs/hooks/endpoints.md#get-v1hooks)

##### Example

```javascript
await hooks.ls({token: 'myregistrytoken'})

=>
[
  { id: '16f7xoal', ... },
  { id: 'wnyf98a1', ... },
  ...
]
```

#### <a name="ls-stream"></a> `> hooks.ls.stream([opts]) -> Stream`

Returns a stream of hook objects associated with the account you're
authenticated as. The returned stream is a valid `Symbol.asyncIterator` on
`node@>=10`.

Results can be further filtered with three values that can be passed in through
`opts`:

* `opts.package` - filter results by package name
* `opts.limit` - maximum number of hooks to return
* `opts.offset` - pagination offset for results (use with `opts.limit`)

See also:
  * [`hooks.ls()`](#ls)
  * [`GET
/v1/hooks`](https://github.com/npm/registry/blob/master/docs/hooks/endpoints.md#get-v1hooks)

##### Example

```javascript
for await (let hook of hooks.ls.stream({token: 'myregistrytoken'})) {
  console.log('found hook:', hook.id)
}

=>
// outputs:
// found hook: 16f7xoal
// found hook: wnyf98a1
```
