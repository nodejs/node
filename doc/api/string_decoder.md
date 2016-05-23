# StringDecoder

    Stability: 2 - Stable

The `string_decoder` module provides an API for decoding `Buffer` objects into
strings in a manner that preserves encoded multi-byte UTF-8 and UTF-16
characters. It can be accessed using:

```js
const StringDecoder = require('string_decoder').StringDecoder;
```

The following example shows the basic use of the `StringDecoder` class. 

```js
const StringDecoder = require('string_decoder').StringDecoder;
const decoder = new StringDecoder('utf8');

const cent = Buffer.from([0xC2, 0xA2]);
console.log(decoder.write(cent));

const euro = Buffer.from([0xE2, 0x82, 0xAC]);
console.log(decoder.write(euro));
```

When a `Buffer` instance is written to the `StringDecoder` instance, an
internal buffer is used to ensure that the decoded string does not contain
any partial multibyte-characters. These are held in the buffer until the
next call to `stringDecoder.write()` or until `stringDecoder.end()` is called.

For following example, for instance, writes the three bytes for the European
euro symbol over three separate operations:

```js
const StringDecoder = require('string_decoder').StringDecoder;
const decoder = new StringDecoder('utf8');

decoder.write(Buffer.from([0xE2]));
decoder.write(Buffer.from([0x82]));
console.log(decoder.write(Buffer.from([0xAC])));
```

## Class: new StringDecoder([encoding])
<!-- YAML
added: v0.1.99
-->

* `encoding` {string} The character encoding the `StringDecoder` will use.
  Defaults to `'utf8'`.

Creates a new `StringDecoder` instance.

### stringDecoder.end()
<!-- YAML
added: v0.9.3
-->

Returns any trailing bytes that were left in the buffer as a string. Bytes that
represent partial UTF-8 and UTF-16 characters will be replaced with
substitution characters appropriate for the character encoding.

### stringDecoder.write(buffer)
<!-- YAML
added: v0.1.99
-->

* `buffer` {Buffer} A `Buffer` instance.

Returns a decoded string, ensuring that any partial multibyte characters at the
end of the `Buffer` omitted from the returned string and stored in an internal
buffer for the next call to `stringDecoder.write()` or `stringDecoder.end()`.
