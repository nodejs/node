![boom Logo](https://raw.github.com/hapijs/boom/master/images/boom.png)

HTTP-friendly error objects

[![Build Status](https://secure.travis-ci.org/hapijs/boom.svg)](http://travis-ci.org/hapijs/boom)
[![Current Version](https://img.shields.io/npm/v/boom.svg)](https://www.npmjs.com/package/boom)

Lead Maintainer: [Adam Bretz](https://github.com/arb)

<!-- toc -->

- [Boom](#boom)
  - [Helper Methods](#helper-methods)
    - [`boomify(error, [options])`](#boomifyerror-options)
    - [`wrap(error, [statusCode], [message])`](#wraperror-statuscode-message)
    - [`create(statusCode, [message], [data])`](#createstatuscode-message-data)
  - [HTTP 4xx Errors](#http-4xx-errors)
    - [`Boom.badRequest([message], [data])`](#boombadrequestmessage-data)
    - [`Boom.unauthorized([message], [scheme], [attributes])`](#boomunauthorizedmessage-scheme-attributes)
    - [`Boom.paymentRequired([message], [data])`](#boompaymentrequiredmessage-data)
    - [`Boom.forbidden([message], [data])`](#boomforbiddenmessage-data)
    - [`Boom.notFound([message], [data])`](#boomnotfoundmessage-data)
    - [`Boom.methodNotAllowed([message], [data], [allow])`](#boommethodnotallowedmessage-data-allow)
    - [`Boom.notAcceptable([message], [data])`](#boomnotacceptablemessage-data)
    - [`Boom.proxyAuthRequired([message], [data])`](#boomproxyauthrequiredmessage-data)
    - [`Boom.clientTimeout([message], [data])`](#boomclienttimeoutmessage-data)
    - [`Boom.conflict([message], [data])`](#boomconflictmessage-data)
    - [`Boom.resourceGone([message], [data])`](#boomresourcegonemessage-data)
    - [`Boom.lengthRequired([message], [data])`](#boomlengthrequiredmessage-data)
    - [`Boom.preconditionFailed([message], [data])`](#boompreconditionfailedmessage-data)
    - [`Boom.entityTooLarge([message], [data])`](#boomentitytoolargemessage-data)
    - [`Boom.uriTooLong([message], [data])`](#boomuritoolongmessage-data)
    - [`Boom.unsupportedMediaType([message], [data])`](#boomunsupportedmediatypemessage-data)
    - [`Boom.rangeNotSatisfiable([message], [data])`](#boomrangenotsatisfiablemessage-data)
    - [`Boom.expectationFailed([message], [data])`](#boomexpectationfailedmessage-data)
    - [`Boom.teapot([message], [data])`](#boomteapotmessage-data)
    - [`Boom.badData([message], [data])`](#boombaddatamessage-data)
    - [`Boom.locked([message], [data])`](#boomlockedmessage-data)
    - [`Boom.preconditionRequired([message], [data])`](#boompreconditionrequiredmessage-data)
    - [`Boom.tooManyRequests([message], [data])`](#boomtoomanyrequestsmessage-data)
    - [`Boom.illegal([message], [data])`](#boomillegalmessage-data)
  - [HTTP 5xx Errors](#http-5xx-errors)
    - [`Boom.badImplementation([message], [data])` - (*alias: `internal`*)](#boombadimplementationmessage-data---alias-internal)
    - [`Boom.notImplemented([message], [data])`](#boomnotimplementedmessage-data)
    - [`Boom.badGateway([message], [data])`](#boombadgatewaymessage-data)
    - [`Boom.serverUnavailable([message], [data])`](#boomserverunavailablemessage-data)
    - [`Boom.gatewayTimeout([message], [data])`](#boomgatewaytimeoutmessage-data)
  - [F.A.Q.](#faq)

<!-- tocstop -->

# Boom

**boom** provides a set of utilities for returning HTTP errors. Each utility returns a `Boom` error response
object (instance of `Error`) which includes the following properties:
- `isBoom` - if `true`, indicates this is a `Boom` object instance.
- `isServer` - convenience bool indicating status code >= 500.
- `message` - the error message.
- `typeof` - the contructor used to create the error (e.g. `Boom.badRequest`).
- `output` - the formatted response. Can be directly manipulated after object construction to return a custom
  error response. Allowed root keys:
    - `statusCode` - the HTTP status code (typically 4xx or 5xx).
    - `headers` - an object containing any HTTP headers where each key is a header name and value is the header content.
    - `payload` - the formatted object used as the response payload (stringified). Can be directly manipulated but any
      changes will be lost
      if `reformat()` is called. Any content allowed and by default includes the following content:
        - `statusCode` - the HTTP status code, derived from `error.output.statusCode`.
        - `error` - the HTTP status message (e.g. 'Bad Request', 'Internal Server Error') derived from `statusCode`.
        - `message` - the error message derived from `error.message`.
- inherited `Error` properties.

The `Boom` object also supports the following method:
- `reformat()` - rebuilds `error.output` using the other object properties.

## Helper Methods

### `boomify(error, [options])`

Decorates an error with the **boom** properties where:
- `error` - the `Error` object to decorate.
- `options` - optional object with the following optional settings:
	- `statusCode` - the HTTP status code. Defaults to `500` if no status code is already set.
	- `message` - error message string. If the error already has a message, the provided `message` is added as a prefix.
	  Defaults to no message.
	- `override` - if `false`, the `error` provided is a **boom** object, and a `statusCode` or `message` are provided,
	  the values are ignored. Defaults to `true` (apply the provided `statusCode` and `message` options to the error
	  regardless of its type, `Error` or **boom** object).

Note: This method replaces the [`wrap()`](#wraperror-statuscode-message) and changes the default behavior to override
existing **boom** objects with the provided status code and message.

```js
var error = new Error('Unexpected input');
Boom.boomify(error, { statusCode: 400 });
```

### `wrap(error, [statusCode], [message])`

Note: This method is deprecated.

Decorates an error with the **boom** properties where:
- `error` - the error object to wrap. If `error` is already a **boom** object, returns back the same object.
- `statusCode` - optional HTTP status code. Defaults to `500`.
- `message` - optional message string. If the error already has a message, it adds the message as a prefix.
  Defaults to no message.

```js
var error = new Error('Unexpected input');
Boom.wrap(error, 400);
```

### `create(statusCode, [message], [data])`

Generates an `Error` object with the **boom** decorations where:
- `statusCode` - an HTTP error code number. Must be greater or equal 400.
- `message` - optional message string.
- `data` - additional error data set to `error.data` property.

```js
var error = Boom.create(400, 'Bad request', { timestamp: Date.now() });
```

## HTTP 4xx Errors

### `Boom.badRequest([message], [data])`

Returns a 400 Bad Request error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.badRequest('invalid query');
```

Generates the following response payload:

```json
{
    "statusCode": 400,
    "error": "Bad Request",
    "message": "invalid query"
}
```

### `Boom.unauthorized([message], [scheme], [attributes])`

Returns a 401 Unauthorized error where:
- `message` - optional message.
- `scheme` can be one of the following:
  - an authentication scheme name
  - an array of string values. These values will be separated by ', ' and set to the 'WWW-Authenticate' header.
- `attributes` - an object of values to use while setting the 'WWW-Authenticate' header. This value is only used
  when `scheme` is a string, otherwise it is ignored. Every key/value pair will be included in the
  'WWW-Authenticate' in the format of 'key="value"' as well as in the response payload under the `attributes` key.  Alternatively value can be a string which is use to set the value of the scheme, for example setting the token value for negotiate header.  If string is used message parameter must be null.
  `null` and `undefined` will be replaced with an empty string. If `attributes` is set, `message` will be used as
  the 'error' segment of the 'WWW-Authenticate' header. If `message` is unset, the 'error' segment of the header
  will not be present and `isMissing` will be true on the error object.

If either `scheme` or `attributes` are set, the resultant `Boom` object will have the 'WWW-Authenticate' header set for the response.

```js
Boom.unauthorized('invalid password');
```

Generates the following response:

```json
"payload": {
    "statusCode": 401,
    "error": "Unauthorized",
    "message": "invalid password"
},
"headers" {}
```

```js
Boom.unauthorized('invalid password', 'sample');
```

Generates the following response:

```json
"payload": {
    "statusCode": 401,
    "error": "Unauthorized",
    "message": "invalid password",
    "attributes": {
        "error": "invalid password"
    }
},
"headers" {
  "WWW-Authenticate": "sample error=\"invalid password\""
}
```

```js
Boom.unauthorized(null, 'Negotiate', 'VGhpcyBpcyBhIHRlc3QgdG9rZW4=');
```

Generates the following response:

```json
"payload": {
    "statusCode": 401,
    "error": "Unauthorized",
    "attributes": "VGhpcyBpcyBhIHRlc3QgdG9rZW4="
},
"headers" {
  "WWW-Authenticate": "Negotiate VGhpcyBpcyBhIHRlc3QgdG9rZW4="
}
```

```js
Boom.unauthorized('invalid password', 'sample', { ttl: 0, cache: null, foo: 'bar' });
```

Generates the following response:

```json
"payload": {
    "statusCode": 401,
    "error": "Unauthorized",
    "message": "invalid password",
    "attributes": {
        "error": "invalid password",
        "ttl": 0,
        "cache": "",
        "foo": "bar"
    }
},
"headers" {
  "WWW-Authenticate": "sample ttl=\"0\", cache=\"\", foo=\"bar\", error=\"invalid password\""
}
```

### `Boom.paymentRequired([message], [data])`

Returns a 402 Payment Required error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.paymentRequired('bandwidth used');
```

Generates the following response payload:

```json
{
    "statusCode": 402,
    "error": "Payment Required",
    "message": "bandwidth used"
}
```

### `Boom.forbidden([message], [data])`

Returns a 403 Forbidden error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.forbidden('try again some time');
```

Generates the following response payload:

```json
{
    "statusCode": 403,
    "error": "Forbidden",
    "message": "try again some time"
}
```

### `Boom.notFound([message], [data])`

Returns a 404 Not Found error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.notFound('missing');
```

Generates the following response payload:

```json
{
    "statusCode": 404,
    "error": "Not Found",
    "message": "missing"
}
```

### `Boom.methodNotAllowed([message], [data], [allow])`

Returns a 405 Method Not Allowed error where:
- `message` - optional message.
- `data` - optional additional error data.
- `allow` - optional string or array of strings (to be combined and separated by ', ') which is set to the 'Allow' header.

```js
Boom.methodNotAllowed('that method is not allowed');
```

Generates the following response payload:

```json
{
    "statusCode": 405,
    "error": "Method Not Allowed",
    "message": "that method is not allowed"
}
```

### `Boom.notAcceptable([message], [data])`

Returns a 406 Not Acceptable error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.notAcceptable('unacceptable');
```

Generates the following response payload:

```json
{
    "statusCode": 406,
    "error": "Not Acceptable",
    "message": "unacceptable"
}
```

### `Boom.proxyAuthRequired([message], [data])`

Returns a 407 Proxy Authentication Required error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.proxyAuthRequired('auth missing');
```

Generates the following response payload:

```json
{
    "statusCode": 407,
    "error": "Proxy Authentication Required",
    "message": "auth missing"
}
```

### `Boom.clientTimeout([message], [data])`

Returns a 408 Request Time-out error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.clientTimeout('timed out');
```

Generates the following response payload:

```json
{
    "statusCode": 408,
    "error": "Request Time-out",
    "message": "timed out"
}
```

### `Boom.conflict([message], [data])`

Returns a 409 Conflict error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.conflict('there was a conflict');
```

Generates the following response payload:

```json
{
    "statusCode": 409,
    "error": "Conflict",
    "message": "there was a conflict"
}
```

### `Boom.resourceGone([message], [data])`

Returns a 410 Gone error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.resourceGone('it is gone');
```

Generates the following response payload:

```json
{
    "statusCode": 410,
    "error": "Gone",
    "message": "it is gone"
}
```

### `Boom.lengthRequired([message], [data])`

Returns a 411 Length Required error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.lengthRequired('length needed');
```

Generates the following response payload:

```json
{
    "statusCode": 411,
    "error": "Length Required",
    "message": "length needed"
}
```

### `Boom.preconditionFailed([message], [data])`

Returns a 412 Precondition Failed error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.preconditionFailed();
```

Generates the following response payload:

```json
{
    "statusCode": 412,
    "error": "Precondition Failed"
}
```

### `Boom.entityTooLarge([message], [data])`

Returns a 413 Request Entity Too Large error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.entityTooLarge('too big');
```

Generates the following response payload:

```json
{
    "statusCode": 413,
    "error": "Request Entity Too Large",
    "message": "too big"
}
```

### `Boom.uriTooLong([message], [data])`

Returns a 414 Request-URI Too Large error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.uriTooLong('uri is too long');
```

Generates the following response payload:

```json
{
    "statusCode": 414,
    "error": "Request-URI Too Large",
    "message": "uri is too long"
}
```

### `Boom.unsupportedMediaType([message], [data])`

Returns a 415 Unsupported Media Type error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.unsupportedMediaType('that media is not supported');
```

Generates the following response payload:

```json
{
    "statusCode": 415,
    "error": "Unsupported Media Type",
    "message": "that media is not supported"
}
```

### `Boom.rangeNotSatisfiable([message], [data])`

Returns a 416 Requested Range Not Satisfiable error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.rangeNotSatisfiable();
```

Generates the following response payload:

```json
{
    "statusCode": 416,
    "error": "Requested Range Not Satisfiable"
}
```

### `Boom.expectationFailed([message], [data])`

Returns a 417 Expectation Failed error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.expectationFailed('expected this to work');
```

Generates the following response payload:

```json
{
    "statusCode": 417,
    "error": "Expectation Failed",
    "message": "expected this to work"
}
```

### `Boom.teapot([message], [data])`

Returns a 418 I'm a Teapot error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.teapot('sorry, no coffee...');
```

Generates the following response payload:

```json
{
    "statusCode": 418,
    "error": "I'm a Teapot",
    "message": "Sorry, no coffee..."
}
```

### `Boom.badData([message], [data])`

Returns a 422 Unprocessable Entity error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.badData('your data is bad and you should feel bad');
```

Generates the following response payload:

```json
{
    "statusCode": 422,
    "error": "Unprocessable Entity",
    "message": "your data is bad and you should feel bad"
}
```

### `Boom.locked([message], [data])`

Returns a 423 Locked error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.locked('this resource has been locked');
```

Generates the following response payload:

```json
{
    "statusCode": 423,
    "error": "Locked",
    "message": "this resource has been locked"
}
```

### `Boom.preconditionRequired([message], [data])`

Returns a 428 Precondition Required error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.preconditionRequired('you must supply an If-Match header');
```

Generates the following response payload:

```json
{
    "statusCode": 428,
    "error": "Precondition Required",
    "message": "you must supply an If-Match header"
}
```

### `Boom.tooManyRequests([message], [data])`

Returns a 429 Too Many Requests error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.tooManyRequests('you have exceeded your request limit');
```

Generates the following response payload:

```json
{
    "statusCode": 429,
    "error": "Too Many Requests",
    "message": "you have exceeded your request limit"
}
```

### `Boom.illegal([message], [data])`

Returns a 451 Unavailable For Legal Reasons error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.illegal('you are not permitted to view this resource for legal reasons');
```

Generates the following response payload:

```json
{
    "statusCode": 451,
    "error": "Unavailable For Legal Reasons",
    "message": "you are not permitted to view this resource for legal reasons"
}
```

## HTTP 5xx Errors

All 500 errors hide your message from the end user. Your message is recorded in the server log.

### `Boom.badImplementation([message], [data])` - (*alias: `internal`*)

Returns a 500 Internal Server Error error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.badImplementation('terrible implementation');
```

Generates the following response payload:

```json
{
    "statusCode": 500,
    "error": "Internal Server Error",
    "message": "An internal server error occurred"
}
```

### `Boom.notImplemented([message], [data])`

Returns a 501 Not Implemented error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.notImplemented('method not implemented');
```

Generates the following response payload:

```json
{
    "statusCode": 501,
    "error": "Not Implemented",
    "message": "method not implemented"
}
```

### `Boom.badGateway([message], [data])`

Returns a 502 Bad Gateway error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.badGateway('that is a bad gateway');
```

Generates the following response payload:

```json
{
    "statusCode": 502,
    "error": "Bad Gateway",
    "message": "that is a bad gateway"
}
```

### `Boom.serverUnavailable([message], [data])`

Returns a 503 Service Unavailable error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.serverUnavailable('unavailable');
```

Generates the following response payload:

```json
{
    "statusCode": 503,
    "error": "Service Unavailable",
    "message": "unavailable"
}
```

### `Boom.gatewayTimeout([message], [data])`

Returns a 504 Gateway Time-out error where:
- `message` - optional message.
- `data` - optional additional error data.

```js
Boom.gatewayTimeout();
```

Generates the following response payload:

```json
{
    "statusCode": 504,
    "error": "Gateway Time-out"
}
```

## F.A.Q.

**Q** How do I include extra information in my responses? `output.payload` is missing `data`, what gives?

**A** There is a reason the values passed back in the response payloads are pretty locked down. It's mostly for security and to not leak any important information back to the client. This means you will need to put in a little more effort to include extra information about your custom error. Check out the ["Error transformation"](https://github.com/hapijs/hapi/blob/master/API.md#error-transformation) section in the hapi documentation.

---
