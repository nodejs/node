# StringDecoder

    Stability: 2 - Stable

To use this module, do `require('string_decoder')`. StringDecoder decodes a
buffer to a string. It is a simple interface to `buffer.toString()` but provides
additional support for utf8.

```js
const StringDecoder = require('string_decoder').StringDecoder;
const decoder = new StringDecoder('utf8');

const cent = new Buffer([0xC2, 0xA2]);
console.log(decoder.write(cent));

const euro = new Buffer([0xE2, 0x82, 0xAC]);
console.log(decoder.write(euro));
```

## Class: StringDecoder
<!-- YAML
added: v0.1.99
-->

Accepts a single argument, `encoding` which defaults to `'utf8'`.

### decoder.end()
<!-- YAML
added: v0.9.3
-->

Returns any trailing bytes that were left in the buffer.

### decoder.write(buffer)
<!-- YAML
added: v0.1.99
-->

Returns a decoded string.
