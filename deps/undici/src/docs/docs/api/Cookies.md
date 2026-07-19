# Cookies

<!--introduced_in=v5.15.0-->
<!--type=module-->
<!-- source_link=lib/web/cookies/index.js -->

> Stability: 2 - Stable

A set of helpers for reading and writing HTTP cookies on a [`Headers`][]
instance, following the [RFC 6265bis][] cookie parsing and serialization
algorithm. They operate on the `Cookie` request header and the `Set-Cookie`
response header.

```mjs
import { getCookies, setCookie, deleteCookie, getSetCookies, parseCookie } from 'undici'
```

These functions do not manage a cookie jar or perform any network activity; they
only read from and mutate the supplied `Headers` object.

## `Cookie`

The `Cookie` object describes a single cookie and its attributes. It is the
return shape of [`parseCookie()`][] and [`getSetCookies()`][], and the input
shape accepted by [`setCookie()`][].

* `name` {string} The cookie name.
* `value` {string} The cookie value.
* `expires` {Date|number} The cookie expiry time, as a `Date` or a Unix
  timestamp in seconds. (optional)
* `maxAge` {number} The number of seconds until the cookie expires. (optional)
* `domain` {string} The hosts the cookie is sent to. (optional)
* `path` {string} The URL path that must exist for the cookie to be sent.
  (optional)
* `secure` {boolean} Whether the cookie is only sent over HTTPS. (optional)
* `httpOnly` {boolean} Whether the cookie is inaccessible to client-side
  JavaScript. (optional)
* `sameSite` {string} Controls whether the cookie is sent with cross-site
  requests. One of `'Strict'`, `'Lax'`, or `'None'`. (optional)
* `unparsed` {string[]} Any attributes that were present in the `Set-Cookie`
  header but not recognized, each formatted as `` `name=value` ``. (optional)

## `getCookies(headers)`

<!-- YAML
added: v5.15.0
-->

* `headers` {Headers} The `Headers` to read the `Cookie` header from.
* Returns: {Record<string, string>} A map of cookie names to values.

Parses the `Cookie` header of `headers` and returns its name/value pairs as a
plain object. An empty object is returned when no `Cookie` header is present.

```mjs
import { getCookies, Headers } from 'undici'

const headers = new Headers({
  cookie: 'get=cookies; and=attributes'
})

console.log(getCookies(headers)) // { get: 'cookies', and: 'attributes' }
```

## `deleteCookie(headers, name[, attributes])`

<!-- YAML
added: v5.15.0
-->

* `headers` {Headers} The `Headers` to append the expiring cookie to.
* `name` {string} The name of the cookie to delete.
* `attributes` {Object} (optional)
  * `path` {string} The path the cookie was scoped to. **Default:** `null`.
  * `domain` {string} The domain the cookie was scoped to. **Default:** `null`.
* Returns: {undefined}

Appends a `Set-Cookie` header to `headers` that expires the named cookie by
setting its expiry time to the Unix epoch, causing a client to delete it on
receipt. The optional `path` and `domain` must match those of the cookie being
removed.

```mjs
import { deleteCookie, Headers } from 'undici'

const headers = new Headers()
deleteCookie(headers, 'name')

console.log(headers.get('set-cookie')) // name=; Expires=Thu, 01 Jan 1970 00:00:00 GMT
```

## `getSetCookies(headers)`

<!-- YAML
added: v5.15.0
-->

* `headers` {Headers} The `Headers` to read the `Set-Cookie` headers from.
* Returns: {Cookie[]} The parsed cookies, or an empty array when none are
  present.

Parses every `Set-Cookie` header of `headers` and returns the resulting
[`Cookie`][] objects. A header that fails to parse yields a `null` entry at its
position in the returned array.

```mjs
import { getSetCookies, Headers } from 'undici'

const headers = new Headers({ 'set-cookie': 'undici=getSetCookies; Secure' })

console.log(getSetCookies(headers))
// [
//   {
//     name: 'undici',
//     value: 'getSetCookies',
//     secure: true
//   }
// ]
```

## `setCookie(headers, cookie)`

<!-- YAML
added: v5.15.0
-->

* `headers` {Headers} The `Headers` to append the `Set-Cookie` header to.
* `cookie` {Cookie} The cookie to serialize. See [`Cookie`][].
* Returns: {undefined}

Serializes `cookie` and appends it to the `Set-Cookie` header of `headers`. When
the cookie name is empty, serialization yields no value and no header is
appended.

```mjs
import { setCookie, Headers } from 'undici'

const headers = new Headers()
setCookie(headers, { name: 'undici', value: 'setCookie' })

console.log(headers.get('Set-Cookie')) // undici=setCookie
```

## `parseCookie(cookie)`

<!-- YAML
added: v7.0.0
-->

* `cookie` {string} A `Set-Cookie` header value.
* Returns: {Cookie|null} The parsed cookie, or `null` if `cookie` is invalid.

Parses a single `Set-Cookie` header value into a [`Cookie`][] object. `null` is
returned when the value contains control characters or otherwise cannot be
parsed.

```mjs
import { parseCookie } from 'undici'

console.log(parseCookie('undici=getSetCookies; Secure; SameSite=Lax'))
// {
//   name: 'undici',
//   value: 'getSetCookies',
//   secure: true,
//   sameSite: 'Lax'
// }
```

The cookie value is returned exactly as it appears in the header; percent-encoded
sequences such as `%20` or `%0D%0A` are not decoded. The `sameSite` attribute is
only set when the value is a case-insensitive match for `Strict`, `Lax`, or
`None`.

[RFC 6265bis]: https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-rfc6265bis
[`Cookie`]: #cookie
[`Headers`]: https://developer.mozilla.org/en-US/docs/Web/API/Headers
[`getSetCookies()`]: #getsetcookiesheaders
[`parseCookie()`]: #parsecookiecookie
[`setCookie()`]: #setcookieheaders-cookie
