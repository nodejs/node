# uuid [![Build Status](https://secure.travis-ci.org/kelektiv/node-uuid.svg?branch=master)](http://travis-ci.org/kelektiv/node-uuid) #

Simple, fast generation of [RFC4122](http://www.ietf.org/rfc/rfc4122.txt) UUIDS.

Features:

* Generate RFC4122 version 1 or version 4 UUIDs
* Runs in node.js and browsers
* Cryptographically strong random number generation on supporting platforms
* Small footprint  (Want something smaller? [Check this out](https://gist.github.com/982883)!)

## Quickstart - CommonJS (Recommended)

```shell
npm install uuid
```

```javascript
// Generate a v1 UUID (time-based)
const uuidV1 = require('uuid/v1');
uuidV1(); // -> '6c84fb90-12c4-11e1-840d-7b25c5ee775a'

// Generate a v4 UUID (random)
const uuidV4 = require('uuid/v4');
uuidV4(); // -> '110ec58a-a0f2-4ac4-8393-c866d813b8d1'
```

## Quickstart - Pre-packaged for browsers (Not recommended)

Browser-ready versions of this module are available via [wzrd.in](https://github.com/jfhbrook/wzrd.in).

```html
<script src="http://wzrd.in/standalone/uuid@latest"></script>

<script>
uuid.v1(); // -> v1 UUID
uuid.v4(); // -> v4 UUID
</script>
```

(Note: Do not do this in production.  Just don't.  wzrd.in is a great service, but if you're deploying a "real" service you should be using a packaging tool like browserify or webpack.  If you do go this route you would be well advised to link to a specific version instead of `uuid@latest` to avoid having your code break when we roll out breaking changes.)


## API

### uuid(...)

Generate a V4 uuid. See uuid.v4 documentation below.

### uuid.v1([`options` [, `buffer` [, `offset`]]])

Generate and return a RFC4122 v1 (timestamp-based) UUID.

* `options` - (Object) Optional uuid state to apply. Properties may include:

  * `node` - (Array) Node id as Array of 6 bytes (per 4.1.6). Default: Randomly generated ID.  See note 1.
  * `clockseq` - (Number between 0 - 0x3fff) RFC clock sequence.  Default: An internally maintained clockseq is used.
  * `msecs` - (Number | Date) Time in milliseconds since unix Epoch.  Default: The current time is used.
  * `nsecs` - (Number between 0-9999) additional time, in 100-nanosecond units. Ignored if `msecs` is unspecified. Default: internal uuid counter is used, as per 4.2.1.2.

* `buffer` - (Array | Buffer) Array or buffer where UUID bytes are to be written.
* `offset` - (Number) Starting index in `buffer` at which to begin writing.

Returns `buffer`, if specified, otherwise the string form of the UUID

Notes:

1. The randomly generated node id is only guaranteed to stay constant for the lifetime of the current JS runtime. (Future versions of this module may use persistent storage mechanisms to extend this guarantee.)

Example: Generate string UUID with fully-specified options

```javascript
uuid.v1({
  node: [0x01, 0x23, 0x45, 0x67, 0x89, 0xab],
  clockseq: 0x1234,
  msecs: new Date('2011-11-01').getTime(),
  nsecs: 5678
});   // -> "710b962e-041c-11e1-9234-0123456789ab"
```

Example: In-place generation of two binary IDs

```javascript
// Generate two ids in an array
const arr = new Array(32); // -> []
uuid.v1(null, arr, 0);   // -> [02 a2 ce 90 14 32 11 e1 85 58 0b 48 8e 4f c1 15]
uuid.v1(null, arr, 16);  // -> [02 a2 ce 90 14 32 11 e1 85 58 0b 48 8e 4f c1 15 02 a3 1c b0 14 32 11 e1 85 58 0b 48 8e 4f c1 15]
```

### uuid.v4([`options` [, `buffer` [, `offset`]]])

Generate and return a RFC4122 v4 UUID.

* `options` - (Object) Optional uuid state to apply. Properties may include:

  * `random` - (Number[16]) Array of 16 numbers (0-255) to use in place of randomly generated values
  * `rng` - (Function) Random # generator to use.  Set to one of the built-in generators - `uuid.mathRNG` (all platforms), `uuid.nodeRNG` (node.js only), `uuid.whatwgRNG` (WebKit only) - or a custom function that returns an array[16] of byte values.

* `buffer` - (Array | Buffer) Array or buffer where UUID bytes are to be written.
* `offset` - (Number) Starting index in `buffer` at which to begin writing.

Returns `buffer`, if specified, otherwise the string form of the UUID

Example: Generate string UUID with fully-specified options

```javascript
uuid.v4({
  random: [
    0x10, 0x91, 0x56, 0xbe, 0xc4, 0xfb, 0xc1, 0xea,
    0x71, 0xb4, 0xef, 0xe1, 0x67, 0x1c, 0x58, 0x36
  ]
});
// -> "109156be-c4fb-41ea-b1b4-efe1671c5836"
```

Example: Generate two IDs in a single buffer

```javascript
const buffer = new Array(32); // (or 'new Buffer' in node.js)
uuid.v4(null, buffer, 0);
uuid.v4(null, buffer, 16);
```

## Testing

```
npm test
```

## Legacy node-uuid package

The code for the legacy node-uuid package is available in the `node-uuid` branch.
