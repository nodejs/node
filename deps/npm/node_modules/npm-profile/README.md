# npm-profile

Provides functions for fetching and updating an npmjs.com profile.

```js
const profile = require('npm-profile')
profile.get(registry, {token}).then(result => {
   // …
})
```

The API that this implements is documented here:

* [authentication](https://github.com/npm/registry/blob/master/docs/user/authentication.md)
* [profile editing](https://github.com/npm/registry/blob/master/docs/user/profile.md) (and two-factor authentication)

## Functions

### profile.adduser(username, email, password, config) → Promise

```js
profile.adduser(username, email, password, {registry}).then(result => {
  // do something with result.token
})
```

Creates a new user on the server along with a fresh bearer token for future
authentication as this user.  This is what you see as an `authToken` in an
`.npmrc`.

If the user already exists then the npm registry will return an error, but
this is registry specific and not guaranteed.

* `username` String
* `email` String
* `password` String
* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **Promise Value**

An object with a `token` property that can be passed into future authentication requests.

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be `'E'` followed by the HTTP response code, for
example a Forbidden response would be `E403`.

### profile.login(username, password, config) → Promise

```js
profile.login(username, password, {registry}).catch(err => {
  if (err.code === 'otp') {
    return getOTPFromSomewhere().then(otp => {
      return profile.login(username, password, {registry, auth: {otp}})
    })
  }
}).then(result => {
  // do something with result.token
})
```

Logs you into an existing user.  Does not create the user if they do not
already exist.  Logging in means generating a new bearer token for use in
future authentication. This is what you use as an `authToken` in an `.npmrc`.
 
* `username` String
* `email` String
* `password` String
* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `auth` Object, properties: `otp` — the one-time password from a two-factor
    authentication device.
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **Promise Value**

An object with a `token` property that can be passed into future authentication requests.

#### **Promise Rejection**

An error object indicating what went wrong.

If the object has a `code` property set to `EOTP` then that indicates that
this account must use two-factor authentication to login.  Try again with a
one-time password.

If the object has a `code` property set to `EAUTHIP` then that indicates that
this account is only allowed to login from certain networks and this ip is
not on one of those networks.

If the error was neither of these then the error object will have a
`code` property set to the HTTP response code and a `headers` property with
the HTTP headers in the response.

### profile.get(config) → Promise

```js
profile.get(registry, {auth: {token}}).then(userProfile => {
  // do something with userProfile
})
```

Fetch profile information for the authenticated user.
 
* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `auth` Object, properties: `token` — a bearer token returned from
    `adduser`, `login` or `createToken`, or, `username`, `password` (and
    optionally `otp`).  Authenticating for this command via a username and
    password will likely not be supported in the future.
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **Promise Value**

An object that looks like this:

```js
// "*" indicates a field that may not always appear
{
  tfa: null |
       false |
       {"mode": "auth-only", pending: Boolean} |
       ["recovery", "codes"] |
       "otpauth://...",
  name: String,
  email: String,
  email_verified: Boolean,
  created: Date,
  updated: Date,
  cidr_whitelist: null | ["192.168.1.1/32", ...],
  fullname: String, // *
  homepage: String, // *
  freenode: String, // *
  twitter: String,  // *
  github: String    // *
}
```

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be the HTTP response code.

### profile.set(profileData, config) → Promise

```js
profile.set({github: 'great-github-account-name'}, {registry, auth: {token}})
```

Update profile information for the authenticated user.

* `profileData` An object, like that returned from `profile.get`, but see
  below for caveats relating to `password`, `tfa` and `cidr_whitelist`.
* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `auth` Object, properties: `token` — a bearer token returned from
    `adduser`, `login` or `createToken`, or, `username`, `password` (and
    optionally `otp`).  Authenticating for this command via a username and
    password will likely not be supported in the future.
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **SETTING `password`**

This is used to change your password and is not visible (for obvious
reasons) through the `get()` API.  The value should be an object with `old`
and `new` properties, where the former has the user's current password and
the latter has the desired new password. For example

```js
profile.set({password: {old: 'abc123', new: 'my new (more secure) password'}}, {registry, auth: {token}})
```

#### **SETTING `cidr_whitelist`**

The value for this is an Array.  Only valid CIDR ranges are allowed in it.
Be very careful as it's possible to lock yourself out of your account with
this.  This is not currently exposed in `npm` itself.

```js
profile.set({cidr_whitelist: [ '8.8.8.8/32' ], {registry, auth: {token}})
// ↑ only one of google's dns servers can now access this account.
```

#### **SETTING `tfa`**

Enabling two-factor authentication is a multi-step process.

1. Call `profile.get` and check the status of `tfa`. If `pending` is true then
   you'll need to disable it with `profile.set({tfa: {password, mode: 'disable'}, …)`.
2. `profile.set({tfa: {password, mode}}, {registry, auth: {token}})`
   * Note that the user's `password` is required here in the `tfa` object,
     regardless of how you're authenticating.
   * `mode` is either `auth-only` which requires an `otp` when calling `login`
     or `createToken`, or `mode` is `auth-and-writes` and an `otp` will be
     required on login, publishing or when granting others access to your
     modules.
   * Be aware that this set call may require otp as part of the auth object.
     If otp is needed it will be indicated through a rejection in the usual
     way.
3. If tfa was already enabled then you're just switch modes and a
   successful response means that you're done.  If the tfa property is empty
   and tfa _wasn't_ enabled then it means they were in a pending state.
3. The response will have a `tfa` property set to an `otpauth` URL, as
   [used by Google Authenticator](https://github.com/google/google-authenticator/wiki/Key-Uri-Format).
   You will need to show this to the user for them to add to their
   authenticator application.  This is typically done as a QRCODE, but you
   can also show the value of the `secret` key in the `otpauth` query string
   and they can type or copy paste that in.
4. To complete setting up two factor auth you need to make a second call to
   `profile.set` with `tfa` set to an array of TWO codes from the user's
   authenticator, eg: `profile.set(tfa: [otp1, otp2]}, registry, {token})`
5. On success you'll get a result object with a `tfa` property that has an
   array of one-time-use recovery codes.  These are used to authenticate
   later if the second factor is lost and generally should be printed and
   put somewhere safe.

Disabling two-factor authentication is more straightforward, set the `tfa`
attribute to an object with a `password` property and a `mode` of `disable`.

```js
profile.set({tfa: {password, mode: 'disable'}, {registry, auth: {token}}}
```

#### **Promise Value**

An object reflecting the changes you made, see description for `profile.get`.

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be the HTTP response code.

### profile.listTokens(config) → Promise

```js
profile.listTokens(registry, {token}).then(tokens => {
  // do something with tokens
})
```

Fetch a list of all of the authentication tokens the authenticated user has.

* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `auth` Object, properties: `token` — a bearer token returned from
    `adduser`, `login` or `createToken`, or, `username`, `password` (and
    optionally `otp`).  Authenticating for this command via a username and
    password will likely not be supported in the future.
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **Promise Value**

An array of token objects. Each token object has the following properties:

* key — A sha512 that can be used to remove this token.
* token — The first six characters of the token UUID.  This should be used
  by the user to identify which token this is.
* created — The date and time the token was created
* readonly — If true, this token can only be used to download private modules. Critically, it CAN NOT be used to publish.
* cidr_whitelist — An array of CIDR ranges that this token is allowed to be used from.

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be the HTTP response code.

### profile.removeToken(token|key, config) → Promise

```js
profile.removeToken(key, registry, {token}).then(() => {
  // token is gone!
})
```

Remove a specific authentication token.

* `token|key` String, either a complete authentication token or the key returned by `profile.listTokens`.
* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `auth` Object, properties: `token` — a bearer token returned from
    `adduser`, `login` or `createToken`, or, `username`, `password` (and
    optionally `otp`).  Authenticating for this command via a username and
    password will likely not be supported in the future.
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **Promise Value**

No value.

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be the HTTP response code.

### profile.createToken(password, readonly, cidr_whitelist, config) → Promise

```js
profile.createToken(password, readonly, cidr_whitelist, registry, {token, otp}).then(newToken => {
  // do something with the newToken
})
```

Create a new authentication token, possibly with restrictions.

* `password` String
* `readonly` Boolean
* `cidr_whitelist` Array
* `config` Object
  * `registry` String (for reference, the npm registry is `https://registry.npmjs.org`)
  * `auth` Object, properties: `token` — a bearer token returned from
    `adduser`, `login` or `createToken`, or, `username`, `password` (and
    optionally `otp`).  Authenticating for this command via a username and
    password will likely not be supported in the future.
  * `opts` Object, [make-fetch-happen options](https://www.npmjs.com/package/make-fetch-happen#extra-options) for setting
    things like cache, proxy, SSL CA and retry rules.

#### **Promise Value**

The promise will resolve with an object very much like the one's returned by
`profile.listTokens`.  The only difference is that `token` is not truncated.

```
{
  token: String,
  key: String,    // sha512 hash of the token UUID
  cidr_whitelist: [String],
  created: Date,
  readonly: Boolean
}
```

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be the HTTP response code.

## Logging

This modules logs by emitting `log` events on the global `process` object.
These events look like this:

```
process.emit('log', 'loglevel', 'feature', 'message part 1', 'part 2', 'part 3', 'etc')
```

`loglevel` can be one of: `error`, `warn`, `notice`, `http`, `timing`, `info`, `verbose`, and `silly`.

`feature` is any brief string that describes the component doing the logging.

The remaining arguments are evaluated like `console.log` and joined together with spaces.

A real world example of this is:

```
  process.emit('log', 'http', 'request', '→',conf.method || 'GET', conf.target)
```

To handle the log events, you would do something like this:

```
const log = require('npmlog')
process.on('log', function (level) {
  return log[level].apply(log, [].slice.call(arguments, 1))
})
```
