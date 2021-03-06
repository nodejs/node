# npm-profile

Provides functions for fetching and updating an npmjs.com profile.

```js
const profile = require('npm-profile')
const result = await profile.get(registry, {token})
//...
```

The API that this implements is documented here:

* [authentication](https://github.com/npm/registry/blob/master/docs/user/authentication.md)
* [profile editing](https://github.com/npm/registry/blob/master/docs/user/profile.md) (and two-factor authentication)

## Table of Contents

* [API](#api)
  * Login and Account Creation
    * [`adduser()`](#adduser)
    * [`login()`](#login)
    * [`adduserWeb()`](#adduser-web)
    * [`loginWeb()`](#login-web)
    * [`adduserCouch()`](#adduser-couch)
    * [`loginCouch()`](#login-couch)
  * Profile Data Management
    * [`get()`](#get)
    * [`set()`](#set)
  * Token Management
    * [`listTokens()`](#list-tokens)
    * [`removeToken()`](#remove-token)
    * [`createToken()`](#create-token)

## API

### <a name="adduser"></a> `> profile.adduser(opener, prompter, [opts]) → Promise`

Tries to create a user new web based login, if that fails it falls back to
using the legacy CouchDB APIs.

* `opener` Function (url) → Promise, returns a promise that resolves after a browser has been opened for the user at `url`.
* `prompter` Function (creds) → Promise, returns a promise that resolves to an object with `username`, `email` and `password` properties.

#### **Promise Value**

An object with the following properties:

* `token` String, to be used to authenticate further API calls
* `username` String, the username the user authenticated as

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be `'E'` followed by the HTTP response code, for
example a Forbidden response would be `E403`.

### <a name="login"></a> `> profile.login(opener, prompter, [opts]) → Promise`

Tries to login using new web based login, if that fails it falls back to
using the legacy CouchDB APIs.

* `opener` Function (url) → Promise, returns a promise that resolves after a browser has been opened for the user at `url`.
* `prompter` Function (creds) → Promise, returns a promise that resolves to an object with `username`, and `password` properties.

#### **Promise Value**

An object with the following properties:

* `token` String, to be used to authenticate further API calls
* `username` String, the username the user authenticated as

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.  This error code can only come from a legacy CouchDB login and so
this should be retried with loginCouch.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be `'E'` followed by the HTTP response code, for
example a Forbidden response would be `E403`.

### <a name="adduser-web"></a> `> profile.adduserWeb(opener, [opts]) → Promise`

Tries to create a user new web based login, if that fails it falls back to
using the legacy CouchDB APIs.

* `opener` Function (url) → Promise, returns a promise that resolves after a browser has been opened for the user at `url`.
* [`opts`](#opts) Object

#### **Promise Value**

An object with the following properties:

* `token` String, to be used to authenticate further API calls
* `username` String, the username the user authenticated as

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the registry does not support web-login then an error will be thrown with
its `code` property set to `ENYI` . You should retry with `adduserCouch`.
If you use `adduser` then this fallback will be done automatically.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be `'E'` followed by the HTTP response code, for
example a Forbidden response would be `E403`.

### <a name="login-web"></a> `> profile.loginWeb(opener, [opts]) → Promise`

Tries to login using new web based login, if that fails it falls back to
using the legacy CouchDB APIs.

* `opener` Function (url) → Promise, returns a promise that resolves after a browser has been opened for the user at `url`.
* [`opts`](#opts) Object (optional)

#### **Promise Value**

An object with the following properties:

* `token` String, to be used to authenticate further API calls
* `username` String, the username the user authenticated as

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the registry does not support web-login then an error will be thrown with
its `code` property set to `ENYI` . You should retry with `loginCouch`.
If you use `login` then this fallback will be done automatically.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be `'E'` followed by the HTTP response code, for
example a Forbidden response would be `E403`.

### <a name="adduser-couch"></a> `> profile.adduserCouch(username, email, password, [opts]) → Promise`

```js
const {token} = await profile.adduser(username, email, password, {registry})
// `token` can be passed in through `opts` for authentication.
```

Creates a new user on the server along with a fresh bearer token for future
authentication as this user.  This is what you see as an `authToken` in an
`.npmrc`.

If the user already exists then the npm registry will return an error, but
this is registry specific and not guaranteed.

* `username` String
* `email` String
* `password` String
* [`opts`](#opts) Object (optional)

#### **Promise Value**

An object with the following properties:

* `token` String, to be used to authenticate further API calls
* `username` String, the username the user authenticated as

#### **Promise Rejection**

An error object indicating what went wrong.

The `headers` property will contain the HTTP headers of the response.

If the action was denied because an OTP is required then `code` will be set
to `EOTP`.

If the action was denied because it came from an IP address that this action
on this account isn't allowed from then the `code` will be set to `EAUTHIP`.

Otherwise the code will be `'E'` followed by the HTTP response code, for
example a Forbidden response would be `E403`.

### <a name="login-couch"></a> `> profile.loginCouch(username, password, [opts]) → Promise`

```js
let token
try {
  {token} = await profile.login(username, password, {registry})
} catch (err) {
  if (err.code === 'otp') {
    const otp = await getOTPFromSomewhere()
    {token} = await profile.login(username, password, {otp})
  }
}
// `token` can now be passed in through `opts` for authentication.
```

Logs you into an existing user.  Does not create the user if they do not
already exist.  Logging in means generating a new bearer token for use in
future authentication. This is what you use as an `authToken` in an `.npmrc`.

* `username` String
* `email` String
* `password` String
* [`opts`](#opts) Object (optional)

#### **Promise Value**

An object with the following properties:

* `token` String, to be used to authenticate further API calls
* `username` String, the username the user authenticated as

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

### <a name="get"></a> `> profile.get([opts]) → Promise`

```js
const {name, email} = await profile.get({token})
console.log(`${token} belongs to https://npm.im/~${name}, (mailto:${email})`)
```

Fetch profile information for the authenticated user.

* [`opts`](#opts) Object

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

### <a name="set"></a> `> profile.set(profileData, [opts]) → Promise`

```js
await profile.set({github: 'great-github-account-name'}, {token})
```

Update profile information for the authenticated user.

* `profileData` An object, like that returned from `profile.get`, but see
  below for caveats relating to `password`, `tfa` and `cidr_whitelist`.
* [`opts`](#opts) Object (optional)

#### **SETTING `password`**

This is used to change your password and is not visible (for obvious
reasons) through the `get()` API.  The value should be an object with `old`
and `new` properties, where the former has the user's current password and
the latter has the desired new password. For example

```js
await profile.set({
  password: {
    old: 'abc123',
    new: 'my new (more secure) password'
  }
}, {token})
```

#### **SETTING `cidr_whitelist`**

The value for this is an Array.  Only valid CIDR ranges are allowed in it.
Be very careful as it's possible to lock yourself out of your account with
this.  This is not currently exposed in `npm` itself.

```js
await profile.set({
  cidr_whitelist: [ '8.8.8.8/32' ]
}, {token})
// ↑ only one of google's dns servers can now access this account.
```

#### **SETTING `tfa`**

Enabling two-factor authentication is a multi-step process.

1. Call `profile.get` and check the status of `tfa`. If `pending` is true then
   you'll need to disable it with `profile.set({tfa: {password, mode: 'disable'}, …)`.
2. `profile.set({tfa: {password, mode}}, {registry, token})`
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
   authenticator, eg: `profile.set(tfa: [otp1, otp2]}, {registry, token})`
5. On success you'll get a result object with a `tfa` property that has an
   array of one-time-use recovery codes.  These are used to authenticate
   later if the second factor is lost and generally should be printed and
   put somewhere safe.

Disabling two-factor authentication is more straightforward, set the `tfa`
attribute to an object with a `password` property and a `mode` of `disable`.

```js
await profile.set({tfa: {password, mode: 'disable'}}, {token})
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

### <a name="list-tokens"></a> `> profile.listTokens([opts]) → Promise`

```js
const tokens = await profile.listTokens({registry, token})
console.log(`Number of tokens in your accounts: ${tokens.length}`)
```

Fetch a list of all of the authentication tokens the authenticated user has.

* [`opts`](#opts) Object (optional)

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

### <a name="remove-token"><a> `> profile.removeToken(token|key, opts) → Promise`

```js
await profile.removeToken(key, {token})
// token is gone!
```

Remove a specific authentication token.

* `token|key` String, either a complete authentication token or the key returned by `profile.listTokens`.
* [`opts`](#opts) Object (optional)

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

### <a name="create-token"></a> `> profile.createToken(password, readonly, cidr_whitelist, [opts]) → Promise`

```js
const newToken = await profile.createToken(
  password, readonly, cidr_whitelist, {token, otp}
)
// do something with the newToken
```

Create a new authentication token, possibly with restrictions.

* `password` String
* `readonly` Boolean
* `cidr_whitelist` Array
* [`opts`](#opts) Object Optional

#### **Promise Value**

The promise will resolve with an object very much like the one's returned by
`profile.listTokens`.  The only difference is that `token` is not truncated.

```js
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

### <a name="opts"></a> options objects

The various API functions accept an optional `opts` object as a final
argument.

Options are passed to
[`npm-registry-fetch`
options](https://www.npmjs.com/package/npm-registry-fetch#fetch-opts), so
anything provided to this module will affect the behavior of that one as
well.

Of particular note are `opts.registry`, and the auth-related options:

* `opts.creds` Object, passed through to prompter, common values are:
  * `username` String, default value for username
  * `email` String, default value for email
* `opts.username` and `opts.password` - used for Basic auth
* `opts.otp` String, the two-factor-auth one-time-password (Will prompt for
  this if needed and not provided.)
* `opts.hostname` String, the hostname of the current machine, to show the
  user during the WebAuth flow.  (Defaults to `os.hostname()`.)

## <a name="logging"></a> Logging

This modules logs by emitting `log` events on the global `process` object.
These events look like this:

```js
process.emit('log', 'loglevel', 'feature', 'message part 1', 'part 2', 'part 3', 'etc')
```

`loglevel` can be one of: `error`, `warn`, `notice`, `http`, `timing`, `info`, `verbose`, and `silly`.

`feature` is any brief string that describes the component doing the logging.

The remaining arguments are evaluated like `console.log` and joined together with spaces.

A real world example of this is:

```js
  process.emit('log', 'http', 'request', '→', conf.method || 'GET', conf.target)
```

To handle the log events, you would do something like this:

```js
const log = require('npmlog')
process.on('log', function (level) {
  return log[level].apply(log, [].slice.call(arguments, 1))
})
```
