# StringDecoder

    Stability: 3 - Stable

To use this module, do `require('string_decoder')`. StringDecoder decodes a
buffer to a string. It is a simple interface to `buffer.toString()` but provides
additional support for utf8.

    var StringDecoder = require('string_decoder').StringDecoder;
    var decoder = new StringDecoder('utf8');

    var cent = new Buffer([0xC2, 0xA2]);
    console.log(decoder.write(cent));

    var euro = new Buffer([0xE2, 0x82, 0xAC]);
    console.log(decoder.write(euro));

## Class: StringDecoder

Accepts a single argument, `encoding` which defaults to `utf8`.

### StringDecoder.write(buffer)

Returns a decoded string.