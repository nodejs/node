# Encoding

<!-- introduced_in=REPLACEME -->

> Stability: 1 - Experimental

<!-- source_link=lib/encoding.js -->

The `node:encoding` module provides unicode validation and transcoding.
To access it:

```mjs
import encoding from 'node:encoding';
```

```cjs
const encoding = require('node:encoding');
```

This module is only available under the `node:` scheme. The following will not
work:

```mjs
import encoding from 'encoding';
```

```cjs
const encoding = require('encoding');
```

## `isAscii(input)`

<!-- YAML
added: REPLACEME
-->

* input {Buffer | Uint8Array | string} The ASCII input to validate.
* Returns: {boolean} Returns true if and only if the input is valid ASCII.

This function is used to check if input contains ASCII code points (characters).

## `isUtf8(input)`

<!-- YAML
added: REPLACEME
-->

* input {Buffer | Uint8Array} The UTF8 input to validate.
* Returns: {boolean} Returns true if and only if the input is valid UTF8.

This function is used to check if input contains UTF8 code points (characters).

## `countUtf8(input)`

<!-- YAML
added: REPLACEME
-->

* input {Buffer | Uint8Array}
* Returns: {number}

This function is used to count the number of code points (characters) in the
input assuming that it is a valid UTF8 input.
