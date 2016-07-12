# Buffer

    Stability: 2 - Stable

Prior to the introduction of `TypedArray` in ECMAScript 2015 (ES6), the
JavaScript language had no mechanism for reading or manipulating streams
of binary data. The `Buffer` class was introduced as part of the Node.js
API to make it possible to interact with octet streams in the context of things
like TCP streams and file system operations.

Now that `TypedArray` has been added in ES6, the `Buffer` class implements the
`Uint8Array` API in a manner that is more optimized and suitable for Node.js'
use cases.

Instances of the `Buffer` class are similar to arrays of integers but
correspond to fixed-sized, raw memory allocations outside the V8 heap.
The size of the `Buffer` is established when it is created and cannot be
resized.

The `Buffer` class is a global within Node.js, making it unlikely that one
would need to ever use `require('buffer').Buffer`.

```js
const buf1 = new Buffer(10);
  // creates a buffer of length 10
  // This is the same as Buffer.allocUnsafe(10), and the returned
  // Buffer instance might contain old data that needs to be
  // overwritten using either fill() or write().

const buf2 = new Buffer([1,2,3]);
  // creates a buffer containing [01, 02, 03]
  // This is the same as Buffer.from([1,2,3]).

const buf3 = new Buffer('test');
  // creates a buffer containing ASCII bytes [74, 65, 73, 74]
  // This is the same as Buffer.from('test').

const buf4 = new Buffer('tést', 'utf8');
  // creates a buffer containing UTF8 bytes [74, c3, a9, 73, 74]
  // This is the same as Buffer.from('tést', 'utf8').

const buf5 = Buffer.alloc(10);
  // Creates a zero-filled Buffer of length 10.

const buf6 = Buffer.alloc(10, 1);
  // Creates a Buffer of length 10, filled with 0x01.

const buf7 = Buffer.allocUnsafe(10);
  // Creates an uninitialized buffer of length 10.
  // This is faster than calling Buffer.alloc() but the returned
  // Buffer instance might contain old data that needs to be
  // overwritten using either fill() or write().

const buf8 = Buffer.from([1,2,3]);
  // Creates a Buffer containing [01, 02, 03].

const buf9 = Buffer.from('test');
  // Creates a Buffer containing ASCII bytes [74, 65, 73, 74].

const buf8 = Buffer.from('tést', 'utf8');
  // Creates a Buffer containing UTF8 bytes [74, c3, a9, 73, 74].
```

## `Buffer.from()`, `Buffer.alloc()`, and `Buffer.allocUnsafe()`

Historically, `Buffer` instances have been created using the `Buffer`
constructor function, which allocates the returned `Buffer`
differently based on what arguments are provided:

* Passing a number as the first argument to `Buffer()` (e.g. `new Buffer(10)`),
  allocates a new `Buffer` object of the specified size. The memory allocated
  for such `Buffer` instances is *not* initialized and *can contain sensitive
  data*. Such `Buffer` objects *must* be initialized *manually* by using either
  [`buf.fill(0)`][] or by writing to the `Buffer` completely. While this
  behavior is *intentional* to improve performance, development experience has
  demonstrated that a more explicit distinction is required between creating a
  fast-but-uninitialized `Buffer` versus creating a slower-but-safer `Buffer`.
* Passing a string, array, or `Buffer` as the first argument copies the
  passed object's data into the `Buffer`.
* Passing an `ArrayBuffer` returns a `Buffer` that shares allocated memory with
  the given `ArrayBuffer`.

Because the behavior of `new Buffer()` changes significantly based on the type
of value passed as the first argument, applications that do not properly
validate the input arguments passed to `new Buffer()`, or that fail to
appropriately initialize newly allocated `Buffer` content, can inadvertently
introduce security and reliability issues into their code.

To make the creation of `Buffer` objects more reliable and less error prone,
new `Buffer.from()`, `Buffer.alloc()`, and `Buffer.allocUnsafe()` methods have
been introduced as an alternative means of creating `Buffer` instances.

*Developers should migrate all existing uses of the `new Buffer()` constructors
to one of these new APIs.*

* [`Buffer.from(array)`][buffer_from_array] returns a new `Buffer` containing
  a *copy* of the provided octets.
* [`Buffer.from(arrayBuffer)`][buffer_from_arraybuf] returns a new `Buffer`
  that *shares* the same allocated memory as the given `ArrayBuffer`.
* [`Buffer.from(buffer)`][buffer_from_buffer] returns a new `Buffer`
  containing a *copy* of the contents of the given `Buffer`.
* [`Buffer.from(str[, encoding])`][buffer_from_string] returns a new `Buffer`
  containing a *copy* of the provided string.
* [`Buffer.alloc(size[, fill[, encoding]])`][buffer_alloc] returns a "filled"
  `Buffer` instance of the specified size. This method can be significantly
  slower than [`Buffer.allocUnsafe(size)`][buffer_allocunsafe] but ensures
  that newly created `Buffer` instances never contain old and potentially
  sensitive data.
* [`Buffer.allocUnsafe(size)`][buffer_allocunsafe] and
  [`Buffer.allocUnsafeSlow(size)`][buffer_allocunsafeslow] each return a
  new `Buffer` of the specified `size` whose content *must* be initialized
  using either [`buf.fill(0)`][] or written to completely.

`Buffer` instances returned by `Buffer.allocUnsafe(size)` *may* be allocated
off a shared internal memory pool if `size` is less than or equal to half
`Buffer.poolSize`. Instances returned by `Buffer.allocUnsafeSlow(size)` *never*
use the shared internal memory pool.

### What makes `Buffer.allocUnsafe(size)` and `Buffer.allocUnsafeSlow(size)` "unsafe"?

When calling `Buffer.allocUnsafe()` (and `Buffer.allocUnsafeSlow()`), the
segment of allocated memory is *uninitialized* (it is not zeroed-out). While
this design makes the allocation of memory quite fast, the allocated segment of
memory might contain old data that is potentially sensitive. Using a `Buffer`
created by `Buffer.allocUnsafe()` without *completely* overwriting the memory
can allow this old data to be leaked when the `Buffer` memory is read.

While there are clear performance advantages to using `Buffer.allocUnsafe()`,
extra care *must* be taken in order to avoid introducing security
vulnerabilities into an application.

## Buffers and Character Encodings

Buffers are commonly used to represent sequences of encoded characters
such as UTF8, UCS2, Base64 or even Hex-encoded data. It is possible to
convert back and forth between Buffers and ordinary JavaScript string objects
by using an explicit encoding method.

```js
const buf = new Buffer('hello world', 'ascii');
console.log(buf.toString('hex'));
  // prints: 68656c6c6f20776f726c64
console.log(buf.toString('base64'));
  // prints: aGVsbG8gd29ybGQ=
```

The character encodings currently supported by Node.js include:

* `'ascii'` - for 7-bit ASCII data only.  This encoding method is very fast and
  will strip the high bit if set.

* `'utf8'` - Multibyte encoded Unicode characters. Many web pages and other
  document formats use UTF-8.

* `'utf16le'` - 2 or 4 bytes, little-endian encoded Unicode characters.
  Surrogate pairs (U+10000 to U+10FFFF) are supported.

* `'ucs2'` - Alias of `'utf16le'`.

* `'base64'` - Base64 string encoding. When creating a buffer from a string,
  this encoding will also correctly accept "URL and Filename Safe Alphabet" as
  specified in [RFC 4648, Section 5].

* `'binary'` - A way of encoding the buffer into a one-byte (`latin-1`)
  encoded string. The string `'latin-1'` is not supported. Instead, pass
  `'binary'` to use `'latin-1'` encoding.

* `'hex'` - Encode each byte as two hexadecimal characters.

## Buffers and TypedArray

Buffers are also `Uint8Array` TypedArray instances. However, there are subtle
incompatibilities with the TypedArray specification in ECMAScript 2015. For
instance, while `ArrayBuffer#slice()` creates a copy of the slice,
the implementation of [`Buffer#slice()`][`buf.slice()`] creates a view over the
existing Buffer without copying, making `Buffer#slice()` far more efficient.

It is also possible to create new TypedArray instances from a `Buffer` with the
following caveats:

1. The Buffer instances's memory is copied to the TypedArray, not shared.

2. The Buffer's memory is interpreted as an array of distinct elements, and not
as a byte array of the target type. That is,
`new Uint32Array(new Buffer([1,2,3,4]))` creates a 4-element `Uint32Array`
   with elements `[1,2,3,4]`, not a `Uint32Array` with a single element
   `[0x1020304]` or `[0x4030201]`.

It is possible to create a new Buffer that shares the same allocated memory as
a TypedArray instance by using the TypeArray objects `.buffer` property:

```js
const arr = new Uint16Array(2);
arr[0] = 5000;
arr[1] = 4000;

const buf1 = new Buffer(arr); // copies the buffer
const buf2 = new Buffer(arr.buffer); // shares the memory with arr;

console.log(buf1);
  // Prints: <Buffer 88 a0>, copied buffer has only two elements
console.log(buf2);
  // Prints: <Buffer 88 13 a0 0f>

arr[1] = 6000;
console.log(buf1);
  // Prints: <Buffer 88 a0>
console.log(buf2);
  // Prints: <Buffer 88 13 70 17>
```

Note that when creating a Buffer using the TypeArray's `.buffer`, it is not
currently possible to use only a portion of the underlying `ArrayBuffer`. To
create a Buffer that uses only a part of the `ArrayBuffer`, use the
[`buf.slice()`][] function after the Buffer is created:

```js
const arr = new Uint16Array(20);
const buf = new Buffer(arr.buffer).slice(0, 16);
console.log(buf.length);
  // Prints: 16
```

The `Buffer.from()` and [`TypedArray.from()`][] (e.g.`Uint8Array.from()`) have
different signatures and implementations. Specifically, the TypedArray variants
accept a second argument that is a mapping function that is invoked on every
element of the typed array:

* `TypedArray.from(source[, mapFn[, thisArg]])`

The `Buffer.from()` method, however, does not support the use of a mapping
function:

* [`Buffer.from(array)`][buffer_from_array]
* [`Buffer.from(buffer)`][buffer_from_buffer]
* [`Buffer.from(arrayBuffer)`][buffer_from_arraybuf]
* [`Buffer.from(str[, encoding])`][buffer_from_string]

## Buffers and ES6 iteration

Buffers can be iterated over using the ECMAScript 2015 (ES6) `for..of` syntax:

```js
const buf = new Buffer([1, 2, 3]);

for (var b of buf)
  console.log(b)

// Prints:
//   1
//   2
//   3
```

Additionally, the [`buf.values()`][], [`buf.keys()`][], and
[`buf.entries()`][] methods can be used to create iterators.

## The `--zero-fill-buffers` command line option

Node.js can be started using the `--zero-fill-buffers` command line option to
force all newly allocated `Buffer` and `SlowBuffer` instances created using
either `new Buffer(size)` and `new SlowBuffer(size)` to be *automatically
zero-filled* upon creation. Use of this flag *changes the default behavior* of
these methods and *can have a significant impact* on performance. Use of the
`--zero-fill-buffers` option is recommended only when absolutely necessary to
enforce that newly allocated `Buffer` instances cannot contain potentially
sensitive data.

```
$ node --zero-fill-buffers
> Buffer(5);
<Buffer 00 00 00 00 00>
```

## Class: Buffer

The Buffer class is a global type for dealing with binary data directly.
It can be constructed in a variety of ways.

### new Buffer(array)

* `array` {Array}

Allocates a new Buffer using an `array` of octets.

```js
const buf = new Buffer([0x62,0x75,0x66,0x66,0x65,0x72]);
  // creates a new Buffer containing ASCII bytes
  // ['b','u','f','f','e','r']
```

### new Buffer(buffer)

* `buffer` {Buffer}

Copies the passed `buffer` data onto a new `Buffer` instance.

```js
const buf1 = new Buffer('buffer');
const buf2 = new Buffer(buf1);

buf1[0] = 0x61;
console.log(buf1.toString());
  // 'auffer'
console.log(buf2.toString());
  // 'buffer' (copy is not changed)
```

### new Buffer(arrayBuffer)

* `arrayBuffer` - The `.buffer` property of a `TypedArray` or a `new
  ArrayBuffer()`

When passed a reference to the `.buffer` property of a `TypedArray` instance,
the newly created Buffer will share the same allocated memory as the
TypedArray.

```js
const arr = new Uint16Array(2);
arr[0] = 5000;
arr[1] = 4000;

const buf = new Buffer(arr.buffer); // shares the memory with arr;

console.log(buf);
  // Prints: <Buffer 88 13 a0 0f>

// changing the TypdArray changes the Buffer also
arr[1] = 6000;

console.log(buf);
  // Prints: <Buffer 88 13 70 17>
```

### new Buffer(size)

* `size` {Number}

Allocates a new Buffer of `size` bytes.  The `size` must be less than
or equal to the value of `require('buffer').kMaxLength` (on 64-bit
architectures, `kMaxLength` is `(2^31)-1`). Otherwise, a [`RangeError`][] is
thrown. If a `size` less than 0 is specified, a zero-length Buffer will be
created.

Unlike `ArrayBuffers`, the underlying memory for Buffer instances created in
this way is not initialized. The contents of a newly created `Buffer` are
unknown and could contain sensitive data. Use [`buf.fill(0)`][] to initialize a
Buffer to zeroes.

```js
const buf = new Buffer(5);
console.log(buf);
  // <Buffer 78 e0 82 02 01>
  // (octets will be different, every time)
buf.fill(0);
console.log(buf);
  // <Buffer 00 00 00 00 00>
```

### new Buffer(str[, encoding])

* `str` {String} String to encode.
* `encoding` {String} Default: `'utf8'`

Creates a new Buffer containing the given JavaScript string `str`. If
provided, the `encoding` parameter identifies the strings character encoding.

```js
const buf1 = new Buffer('this is a tést');
console.log(buf1.toString());
  // prints: this is a tést
console.log(buf1.toString('ascii'));
  // prints: this is a tC)st

const buf2 = new Buffer('7468697320697320612074c3a97374', 'hex');
console.log(buf2.toString());
  // prints: this is a tést
```

### Class Method: Buffer.alloc(size[, fill[, encoding]])
<!-- YAML
added: v4.5.0
-->

* `size` {Number}
* `fill` {Value} Default: `undefined`
* `encoding` {String} Default: `utf8`

Allocates a new `Buffer` of `size` bytes. If `fill` is `undefined`, the
`Buffer` will be *zero-filled*.

```js
const buf = Buffer.alloc(5);
console.log(buf);
  // <Buffer 00 00 00 00 00>
```

The `size` must be less than or equal to the value of
`require('buffer').kMaxLength` (on 64-bit architectures, `kMaxLength` is
`(2^31)-1`). Otherwise, a [`RangeError`][] is thrown. If a `size` less than 0
is specified, a zero-length `Buffer` will be created.

If `fill` is specified, the allocated `Buffer` will be initialized by calling
`buf.fill(fill)`. See [`buf.fill()`][] for more information.

```js
const buf = Buffer.alloc(5, 'a');
console.log(buf);
  // <Buffer 61 61 61 61 61>
```

If both `fill` and `encoding` are specified, the allocated `Buffer` will be
initialized by calling `buf.fill(fill, encoding)`. For example:

```js
const buf = Buffer.alloc(11, 'aGVsbG8gd29ybGQ=', 'base64');
console.log(buf);
  // <Buffer 68 65 6c 6c 6f 20 77 6f 72 6c 64>
```

Calling `Buffer.alloc(size)` can be significantly slower than the alternative
`Buffer.allocUnsafe(size)` but ensures that the newly created `Buffer` instance
contents will *never contain sensitive data*.

A `TypeError` will be thrown if `size` is not a number.

### Class Method: Buffer.allocUnsafe(size)
<!-- YAML
added: v4.5.0
-->

* `size` {Number}

Allocates a new *non-zero-filled* `Buffer` of `size` bytes.  The `size` must
be less than or equal to the value of `require('buffer').kMaxLength` (on 64-bit
architectures, `kMaxLength` is `(2^31)-1`). Otherwise, a [`RangeError`][] is
thrown. If a `size` less than 0 is specified, a zero-length `Buffer` will be
created.

The underlying memory for `Buffer` instances created in this way is *not
initialized*. The contents of the newly created `Buffer` are unknown and
*may contain sensitive data*. Use [`buf.fill(0)`][] to initialize such
`Buffer` instances to zeroes.

```js
const buf = Buffer.allocUnsafe(5);
console.log(buf);
  // <Buffer 78 e0 82 02 01>
  // (octets will be different, every time)
buf.fill(0);
console.log(buf);
  // <Buffer 00 00 00 00 00>
```

A `TypeError` will be thrown if `size` is not a number.

Note that the `Buffer` module pre-allocates an internal `Buffer` instance of
size `Buffer.poolSize` that is used as a pool for the fast allocation of new
`Buffer` instances created using `Buffer.allocUnsafe(size)` (and the
`new Buffer(size)` constructor) only when `size` is less than or equal to
`Buffer.poolSize >> 1` (floor of `Buffer.poolSize` divided by two). The default
value of `Buffer.poolSize` is `8192` but can be modified.

Use of this pre-allocated internal memory pool is a key difference between
calling `Buffer.alloc(size, fill)` vs. `Buffer.allocUnsafe(size).fill(fill)`.
Specifically, `Buffer.alloc(size, fill)` will *never* use the internal Buffer
pool, while `Buffer.allocUnsafe(size).fill(fill)` *will* use the internal
Buffer pool if `size` is less than or equal to half `Buffer.poolSize`. The
difference is subtle but can be important when an application requires the
additional performance that `Buffer.allocUnsafe(size)` provides.

### Class Method: Buffer.allocUnsafeSlow(size)
<!-- YAML
added: v4.5.0
-->

* `size` {Number}

Allocates a new *non-zero-filled* and non-pooled `Buffer` of `size` bytes.  The
`size` must be less than or equal to the value of
`require('buffer').kMaxLength` (on 64-bit architectures, `kMaxLength` is
`(2^31)-1`). Otherwise, a [`RangeError`][] is thrown. If a `size` less than 0
is specified, a zero-length `Buffer` will be created.

The underlying memory for `Buffer` instances created in this way is *not
initialized*. The contents of the newly created `Buffer` are unknown and
*may contain sensitive data*. Use [`buf.fill(0)`][] to initialize such
`Buffer` instances to zeroes.

When using `Buffer.allocUnsafe()` to allocate new `Buffer` instances,
allocations under 4KB are, by default, sliced from a single pre-allocated
`Buffer`. This allows applications to avoid the garbage collection overhead of
creating many individually allocated Buffers. This approach improves both
performance and memory usage by eliminating the need to track and cleanup as
many `Persistent` objects.

However, in the case where a developer may need to retain a small chunk of
memory from a pool for an indeterminate amount of time, it may be appropriate
to create an un-pooled Buffer instance using `Buffer.allocUnsafeSlow()` then
copy out the relevant bits.

```js
// need to keep around a few small chunks of memory
const store = [];

socket.on('readable', () => {
  const data = socket.read();
  // allocate for retained data
  const sb = Buffer.allocUnsafeSlow(10);
  // copy the data into the new allocation
  data.copy(sb, 0, 0, 10);
  store.push(sb);
});
```

Use of `Buffer.allocUnsafeSlow()` should be used only as a last resort *after*
a developer has observed undue memory retention in their applications.

A `TypeError` will be thrown if `size` is not a number.

### Class Method: Buffer.byteLength(string[, encoding])

* `string` {String}
* `encoding` {String} Default: `'utf8'`
* Return: {Number}

Returns the actual byte length of a string. This is not the same as
[`String.prototype.length`][] since that returns the number of *characters* in
a string.

Example:

```js
const str = '\u00bd + \u00bc = \u00be';

console.log(`${str}: ${str.length} characters, ` +
            `${Buffer.byteLength(str, 'utf8')} bytes`);

// ½ + ¼ = ¾: 9 characters, 12 bytes
```

### Class Method: Buffer.compare(buf1, buf2)
<!-- YAML
added: v0.11.13
-->

* `buf1` {Buffer}
* `buf2` {Buffer}
* Return: {Number}

Compares `buf1` to `buf2` typically for the purpose of sorting arrays of
Buffers. This is equivalent is calling [`buf1.compare(buf2)`][].

```js
const arr = [Buffer('1234'), Buffer('0123')];
arr.sort(Buffer.compare);
```

### Class Method: Buffer.concat(list[, totalLength])
<!-- YAML
added: v0.7.11
-->

* `list` {Array} List of Buffer objects to concat
* `totalLength` {Number} Total length of the Buffers in the list when concatenated
* Return: {Buffer}

Returns a new Buffer which is the result of concatenating all the Buffers in
the `list` together.

If the list has no items, or if the `totalLength` is 0, then a new zero-length
Buffer is returned.

If `totalLength` is not provided, it is calculated from the Buffers in the
`list`. This, however, adds an additional loop to the function, so it is faster
to provide the length explicitly.

Example: build a single Buffer from a list of three Buffers:

```js
const buf1 = new Buffer(10).fill(0);
const buf2 = new Buffer(14).fill(0);
const buf3 = new Buffer(18).fill(0);
const totalLength = buf1.length + buf2.length + buf3.length;

console.log(totalLength);
const bufA = Buffer.concat([buf1, buf2, buf3], totalLength);
console.log(bufA);
console.log(bufA.length);

// 42
// <Buffer 00 00 00 00 ...>
// 42
```

### Class Method: Buffer.from(array)
<!-- YAML
added: v3.0.0
-->

* `array` {Array}

Allocates a new `Buffer` using an `array` of octets.

```js
const buf = Buffer.from([0x62,0x75,0x66,0x66,0x65,0x72]);
  // creates a new Buffer containing ASCII bytes
  // ['b','u','f','f','e','r']
```

A `TypeError` will be thrown if `array` is not an `Array`.

### Class Method: Buffer.from(arrayBuffer)
<!-- YAML
added: v4.5.0
-->

* `arrayBuffer` {ArrayBuffer} The `.buffer` property of a `TypedArray` or
  a `new ArrayBuffer()`

When passed a reference to the `.buffer` property of a `TypedArray` instance,
the newly created `Buffer` will share the same allocated memory as the
TypedArray.

```js
const arr = new Uint16Array(2);
arr[0] = 5000;
arr[1] = 4000;

const buf = Buffer.from(arr.buffer); // shares the memory with arr;

console.log(buf);
  // Prints: <Buffer 88 13 a0 0f>

// changing the TypedArray changes the Buffer also
arr[1] = 6000;

console.log(buf);
  // Prints: <Buffer 88 13 70 17>
```

A `TypeError` will be thrown if `arrayBuffer` is not an `ArrayBuffer`.

### Class Method: Buffer.from(buffer)
<!-- YAML
added: v3.0.0
-->

* `buffer` {Buffer}

Copies the passed `buffer` data onto a new `Buffer` instance.

```js
const buf1 = Buffer.from('buffer');
const buf2 = Buffer.from(buf1);

buf1[0] = 0x61;
console.log(buf1.toString());
  // 'auffer'
console.log(buf2.toString());
  // 'buffer' (copy is not changed)
```

A `TypeError` will be thrown if `buffer` is not a `Buffer`.

### Class Method: Buffer.from(str[, encoding])
<!-- YAML
added: v4.5.0
-->

* `str` {String} String to encode.
* `encoding` {String} Encoding to use, Default: `'utf8'`

Creates a new `Buffer` containing the given JavaScript string `str`. If
provided, the `encoding` parameter identifies the character encoding.
If not provided, `encoding` defaults to `'utf8'`.

```js
const buf1 = Buffer.from('this is a tést');
console.log(buf1.toString());
  // prints: this is a tést
console.log(buf1.toString('ascii'));
  // prints: this is a tC)st

const buf2 = Buffer.from('7468697320697320612074c3a97374', 'hex');
console.log(buf2.toString());
  // prints: this is a tést
```

A `TypeError` will be thrown if `str` is not a string.

### Class Method: Buffer.isBuffer(obj)

* `obj` {Object}
* Return: {Boolean}

Returns 'true' if `obj` is a Buffer.

### Class Method: Buffer.isEncoding(encoding)
<!-- YAML
added: v0.9.1
-->

* `encoding` {String} The encoding string to test
* Return: {Boolean}

Returns true if the `encoding` is a valid encoding argument, or false
otherwise.

### buf[index]
<!-- YAML
type: property
name: [index]
-->

The index operator `[index]` can be used to get and set the octet at position
`index` in the Buffer. The values refer to individual bytes, so the legal value
range is between `0x00` and `0xFF` (hex) or `0` and `255` (decimal).

Example: copy an ASCII string into a Buffer, one byte at a time:

```js
const str = "Node.js";
const buf = new Buffer(str.length);

for (var i = 0; i < str.length ; i++) {
  buf[i] = str.charCodeAt(i);
}

console.log(buf.toString('ascii'));
  // Prints: Node.js
```

### buf.compare(otherBuffer)
<!-- YAML
added: v0.11.13
-->

* `otherBuffer` {Buffer}
* Return: {Number}

Compares two Buffer instances and returns a number indicating whether `buf`
comes before, after, or is the same as the `otherBuffer` in sort order.
Comparison is based on the actual sequence of bytes in each Buffer.

* `0` is returned if `otherBuffer` is the same as `buf`
* `1` is returned if `otherBuffer` should come *before* `buf` when sorted.
* `-1` is returned if `otherBuffer` should come *after* `buf` when sorted.

```js
const buf1 = new Buffer('ABC');
const buf2 = new Buffer('BCD');
const buf3 = new Buffer('ABCD');

console.log(buf1.compare(buf1));
  // Prints: 0
console.log(buf1.compare(buf2));
  // Prints: -1
console.log(buf1.compare(buf3));
  // Prints: 1
console.log(buf2.compare(buf1));
  // Prints: 1
console.log(buf2.compare(buf3));
  // Prints: 1

[buf1, buf2, buf3].sort(Buffer.compare);
  // produces sort order [buf1, buf3, buf2]
```

### buf.copy(targetBuffer[, targetStart[, sourceStart[, sourceEnd]]])

* `targetBuffer` {Buffer} Buffer to copy into
* `targetStart` {Number} Default: 0
* `sourceStart` {Number} Default: 0
* `sourceEnd` {Number} Default: `buffer.length`
* Return: {Number} The number of bytes copied.

Copies data from a region of this Buffer to a region in the target Buffer even
if the target memory region overlaps with the source.

Example: build two Buffers, then copy `buf1` from byte 16 through byte 19
into `buf2`, starting at the 8th byte in `buf2`.

```js
const buf1 = new Buffer(26);
const buf2 = new Buffer(26).fill('!');

for (var i = 0 ; i < 26 ; i++) {
  buf1[i] = i + 97; // 97 is ASCII a
}

buf1.copy(buf2, 8, 16, 20);
console.log(buf2.toString('ascii', 0, 25));
  // Prints: !!!!!!!!qrst!!!!!!!!!!!!!
```

Example: Build a single Buffer, then copy data from one region to an overlapping
region in the same Buffer

```js
const buf = new Buffer(26);

for (var i = 0 ; i < 26 ; i++) {
  buf[i] = i + 97; // 97 is ASCII a
}

buf.copy(buf, 0, 4, 10);
console.log(buf.toString());

// efghijghijklmnopqrstuvwxyz
```

### buf.entries()
<!-- YAML
added: v1.1.0
-->

* Return: {Iterator}

Creates and returns an [iterator][] of `[index, byte]` pairs from the Buffer
contents.

```js
const buf = new Buffer('buffer');
for (var pair of buf.entries()) {
  console.log(pair);
}
// prints:
//   [0, 98]
//   [1, 117]
//   [2, 102]
//   [3, 102]
//   [4, 101]
//   [5, 114]
```

### buf.equals(otherBuffer)
<!-- YAML
added: v1.0.0
-->

* `otherBuffer` {Buffer}
* Return: {Boolean}

Returns a boolean indicating whether `this` and `otherBuffer` have exactly the
same bytes.

```js
const buf1 = new Buffer('ABC');
const buf2 = new Buffer('414243', 'hex');
const buf3 = new Buffer('ABCD');

console.log(buf1.equals(buf2));
  // Prints: true
console.log(buf1.equals(buf3));
  // Prints: false
```

### buf.fill(value[, offset[, end]])
<!-- YAML
added: v0.5.0
-->

* `value` {String|Number}
* `offset` {Number} Default: 0
* `end` {Number} Default: `buffer.length`
* Return: {Buffer}

Fills the Buffer with the specified value. If the `offset` and `end` are not
given it will fill the entire Buffer. The method returns a reference to the
Buffer so calls can be chained.

```js
const b = new Buffer(50).fill('h');
console.log(b.toString());
  // Prints: hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
```

### buf.indexOf(value[, byteOffset][, encoding])
<!-- YAML
added: v1.5.0
-->

* `value` {String|Buffer|Number}
* `byteOffset` {Number} Default: 0
* `encoding` {String} Default: `'utf8'`
* Return: {Number}

Operates similar to [`Array#indexOf()`][] in that it returns either the
starting index position of `value` in Buffer or `-1` if the Buffer does not
contain `value`. The `value` can be a String, Buffer or Number. Strings are by
default interpreted as UTF8. Buffers will use the entire Buffer (to compare a
partial Buffer use [`buf.slice()`][]).  Numbers can range from 0 to 255.

```js
const buf = new Buffer('this is a buffer');

buf.indexOf('this');
  // returns 0
buf.indexOf('is');
  // returns 2
buf.indexOf(new Buffer('a buffer'));
  // returns 8
buf.indexOf(97); // ascii for 'a'
  // returns 8
buf.indexOf(new Buffer('a buffer example'));
  // returns -1
buf.indexOf(new Buffer('a buffer example').slice(0,8));
  // returns 8

const utf16Buffer = new Buffer('\u039a\u0391\u03a3\u03a3\u0395', 'ucs2');

utf16Buffer.indexOf('\u03a3',  0, 'ucs2');
  // returns 4
utf16Buffer.indexOf('\u03a3', -4, 'ucs2');
  // returns 6
```

### buf.keys()
<!-- YAML
added: v1.1.0
-->

* Return: {Iterator}

Creates and returns an [iterator][] of Buffer keys (indices).

```js
const buf = new Buffer('buffer');
for (var key of buf.keys()) {
  console.log(key);
}
// prints:
//   0
//   1
//   2
//   3
//   4
//   5
```

### buf.length

* {Number}

Returns the amount of memory allocated for the Buffer in number of bytes. Note
that this does not necessarily reflect the amount of usable data within the
Buffer. For instance, in the example below, a Buffer with 1234 bytes is
allocated, but only 11 ASCII bytes are written.

```js
const buf = new Buffer(1234);

console.log(buf.length);
  // Prints: 1234

buf.write('some string', 0, 'ascii');
console.log(buf.length);
  // Prints: 1234
```

While the `length` property is not immutable, changing the value of `length`
can result in undefined and inconsistent behavior. Applications that wish to
modify the length of a Buffer should therefore treat `length` as read-only and
use [`buf.slice()`][] to create a new Buffer.

```js
var buf = new Buffer(10);
buf.write('abcdefghj', 0, 'ascii');
console.log(buf.length);
  // Prints: 10
buf = buf.slice(0,5);
console.log(buf.length);
  // Prints: 5
```

### buf.readDoubleBE(offset[, noAssert])
### buf.readDoubleLE(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 8`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads a 64-bit double from the Buffer at the specified `offset` with specified
endian format (`readDoubleBE()` returns big endian, `readDoubleLE()` returns
little endian).

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

```js
const buf = new Buffer([1,2,3,4,5,6,7,8]);

buf.readDoubleBE();
  // Returns: 8.20788039913184e-304
buf.readDoubleLE();
  // Returns: 5.447603722011605e-270
buf.readDoubleLE(1);
  // throws RangeError: Index out of range

buf.readDoubleLE(1, true); // Warning: reads passed end of buffer!
  // Segmentation fault! don't do this!
```

### buf.readFloatBE(offset[, noAssert])
### buf.readFloatLE(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 4`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads a 32-bit float from the Buffer at the specified `offset` with specified
endian format (`readFloatBE()` returns big endian, `readFloatLE()` returns
little endian).

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

```js
const buf = new Buffer([1,2,3,4]);

buf.readFloatBE();
  // Returns: 2.387939260590663e-38
buf.readFloatLE();
  // Returns: 1.539989614439558e-36
buf.readFloatLE(1);
  // throws RangeError: Index out of range

buf.readFloatLE(1, true); // Warning: reads passed end of buffer!
  // Segmentation fault! don't do this!
```

### buf.readInt8(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 1`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads a signed 8-bit integer from the Buffer at the specified `offset`.

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

Integers read from the Buffer are interpreted as two's complement signed values.

```js
const buf = new Buffer([1,-2,3,4]);

buf.readInt8(0);
  // returns 1
buf.readInt8(1);
  // returns -2
```

### buf.readInt16BE(offset[, noAssert])
### buf.readInt16LE(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 2`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads a signed 16-bit integer from the Buffer at the specified `offset` with
the specified endian format (`readInt16BE()` returns big endian,
`readInt16LE()` returns little endian).

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

Integers read from the Buffer are interpreted as two's complement signed values.

```js
const buf = new Buffer([1,-2,3,4]);

buf.readInt16BE();
  // returns 510
buf.readInt16LE(1);
  // returns 1022
```

### buf.readInt32BE(offset[, noAssert])
### buf.readInt32LE(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 4`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads a signed 32-bit integer from the Buffer at the specified `offset` with
the specified endian format (`readInt32BE()` returns big endian,
`readInt32LE()` returns little endian).

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

Integers read from the Buffer are interpreted as two's complement signed values.

```js
const buf = new Buffer([1,-2,3,4]);

buf.readInt32BE();
  // returns 33424132
buf.readInt32LE();
  // returns 67370497
buf.readInt32LE(1);
  // throws RangeError: Index out of range
```

### buf.readIntBE(offset, byteLength[, noAssert])
### buf.readIntLE(offset, byteLength[, noAssert])
<!-- YAML
added: v1.0.0
-->

* `offset` {Number} `0 <= offset <= buf.length - byteLength`
* `byteLength` {Number} `0 < byteLength <= 6`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads `byteLength` number of bytes from the Buffer at the specified `offset`
and interprets the result as a two's complement signed value. Supports up to 48
bits of accuracy. For example:

```js
const buf = new Buffer(6);
buf.writeUInt16LE(0x90ab, 0);
buf.writeUInt32LE(0x12345678, 2);
buf.readIntLE(0, 6).toString(16);  // Specify 6 bytes (48 bits)
// Returns: '1234567890ab'

buf.readIntBE(0, 6).toString(16);
// Returns: -546f87a9cbee
```

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

### buf.readUInt8(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 1`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads an unsigned 8-bit integer from the Buffer at the specified `offset`.

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

```js
const buf = new Buffer([1,-2,3,4]);

buf.readUInt8(0);
  // returns 1
buf.readUInt8(1);
  // returns 254
```

### buf.readUInt16BE(offset[, noAssert])
### buf.readUInt16LE(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 2`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads an unsigned 16-bit integer from the Buffer at the specified `offset` with
specified endian format (`readUInt16BE()` returns big endian,
`readUInt16LE()` returns little endian).

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

Example:

```js
const buf = new Buffer([0x3, 0x4, 0x23, 0x42]);

buf.readUInt16BE(0);
  // Returns: 0x0304
buf.readUInt16LE(0);
  // Returns: 0x0403
buf.readUInt16BE(1);
  // Returns: 0x0423
buf.readUInt16LE(1);
  // Returns: 0x2304
buf.readUInt16BE(2);
  // Returns: 0x2342
buf.readUInt16LE(2);
  // Returns: 0x4223
```

### buf.readUInt32BE(offset[, noAssert])
### buf.readUInt32LE(offset[, noAssert])

* `offset` {Number} `0 <= offset <= buf.length - 4`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads an unsigned 32-bit integer from the Buffer at the specified `offset` with
specified endian format (`readUInt32BE()` returns big endian,
`readUInt32LE()` returns little endian).

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

Example:

```js
const buf = new Buffer([0x3, 0x4, 0x23, 0x42]);

buf.readUInt32BE(0);
  // Returns: 0x03042342
console.log(buf.readUInt32LE(0));
  // Returns: 0x42230403
```

### buf.readUIntBE(offset, byteLength[, noAssert])
### buf.readUIntLE(offset, byteLength[, noAssert])
<!-- YAML
added: v1.0.0
-->

* `offset` {Number} `0 <= offset <= buf.length - byteLength`
* `byteLength` {Number} `0 < byteLength <= 6`
* `noAssert` {Boolean} Default: false
* Return: {Number}

Reads `byteLength` number of bytes from the Buffer at the specified `offset`
and interprets the result as an unsigned integer. Supports up to 48
bits of accuracy. For example:

```js
const buf = new Buffer(6);
buf.writeUInt16LE(0x90ab, 0);
buf.writeUInt32LE(0x12345678, 2);
buf.readUIntLE(0, 6).toString(16);  // Specify 6 bytes (48 bits)
// Returns: '1234567890ab'

buf.readUIntBE(0, 6).toString(16);
// Returns: ab9078563412
```

Setting `noAssert` to `true` skips validation of the `offset`. This allows the
`offset` to be beyond the end of the Buffer.

### buf.slice([start[, end]])

* `start` {Number} Default: 0
* `end` {Number} Default: `buffer.length`
* Return: {Buffer}

Returns a new Buffer that references the same memory as the original, but
offset and cropped by the `start` and `end` indices.

**Note that modifying the new Buffer slice will modify the memory in the
original Buffer because the allocated memory of the two objects overlap.**

Example: build a Buffer with the ASCII alphabet, take a slice, then modify one
byte from the original Buffer.

```js
const buf1 = new Buffer(26);

for (var i = 0 ; i < 26 ; i++) {
  buf1[i] = i + 97; // 97 is ASCII a
}

const buf2 = buf1.slice(0, 3);
buf2.toString('ascii', 0, buf2.length);
  // Returns: 'abc'
buf1[0] = 33;
buf2.toString('ascii', 0, buf2.length);
  // Returns : '!bc'
```

Specifying negative indexes causes the slice to be generated relative to the
end of the Buffer rather than the beginning.

```js
const buf = new Buffer('buffer');

buf.slice(-6, -1).toString();
  // Returns 'buffe', equivalent to buf.slice(0, 5)
buf.slice(-6, -2).toString();
  // Returns 'buff', equivalent to buf.slice(0, 4)
buf.slice(-5, -2).toString();
  // Returns 'uff', equivalent to buf.slice(1, 4)
```

### buf.toString([encoding[, start[, end]]])

* `encoding` {String} Default: `'utf8'`
* `start` {Number} Default: 0
* `end` {Number} Default: `buffer.length`
* Return: {String}

Decodes and returns a string from the Buffer data using the specified
character set `encoding`.

```js
const buf = new Buffer(26);
for (var i = 0 ; i < 26 ; i++) {
  buf[i] = i + 97; // 97 is ASCII a
}
buf.toString('ascii');
  // Returns: 'abcdefghijklmnopqrstuvwxyz'
buf.toString('ascii',0,5);
  // Returns: 'abcde'
buf.toString('utf8',0,5);
  // Returns: 'abcde'
buf.toString(undefined,0,5);
  // Returns: 'abcde', encoding defaults to 'utf8'
```

### buf.toJSON()
<!-- YAML
added: v0.9.2
-->

* Return: {Object}

Returns a JSON representation of the Buffer instance.  [`JSON.stringify()`][]
implicitly calls this function when stringifying a Buffer instance.

Example:

```js
const buf = new Buffer('test');
const json = JSON.stringify(buf);

console.log(json);
// Prints: '{"type":"Buffer","data":[116,101,115,116]}'

const copy = JSON.parse(json, (key, value) => {
    return value && value.type === 'Buffer'
      ? new Buffer(value.data)
      : value;
  });

console.log(copy.toString());
// Prints: 'test'
```

### buf.values()
<!-- YAML
added: v1.1.0
-->

* Return: {Iterator}

Creates and returns an [iterator][] for Buffer values (bytes). This function is
called automatically when the Buffer is used in a `for..of` statement.

```js
const buf = new Buffer('buffer');
for (var value of buf.values()) {
  console.log(value);
}
// prints:
//   98
//   117
//   102
//   102
//   101
//   114

for (var value of buf) {
  console.log(value);
}
// prints:
//   98
//   117
//   102
//   102
//   101
//   114
```

### buf.write(string[, offset[, length]][, encoding])

* `string` {String} Bytes to be written to buffer
* `offset` {Number} Default: 0
* `length` {Number} Default: `buffer.length - offset`
* `encoding` {String} Default: `'utf8'`
* Return: {Number} Numbers of bytes written

Writes `string` to the Buffer at `offset` using the given `encoding`.
The `length` parameter is the number of bytes to write. If the Buffer did not
contain enough space to fit the entire string, only a partial amount of the
string will be written however, it will not write only partially encoded
characters.

```js
const buf = new Buffer(256);
const len = buf.write('\u00bd + \u00bc = \u00be', 0);
console.log(`${len} bytes: ${buf.toString('utf8', 0, len)}`);
  // Prints: 12 bytes: ½ + ¼ = ¾
```

### buf.writeDoubleBE(value, offset[, noAssert])
### buf.writeDoubleLE(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 8`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` with specified endian
format (`writeDoubleBE()` writes big endian, `writeDoubleLE()` writes little
endian). The `value` argument *should* be a valid 64-bit double. Behavior is
not defined when `value` is anything other than a 64-bit double.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Example:

```js
const buf = new Buffer(8);
buf.writeDoubleBE(0xdeadbeefcafebabe, 0);

console.log(buf);
  // Prints: <Buffer 43 eb d5 b7 dd f9 5f d7>

buf.writeDoubleLE(0xdeadbeefcafebabe, 0);

console.log(buf);
  // Prints: <Buffer d7 5f f9 dd b7 d5 eb 43>
```

### buf.writeFloatBE(value, offset[, noAssert])
### buf.writeFloatLE(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 4`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` with specified endian
format (`writeFloatBE()` writes big endian, `writeFloatLE()` writes little
endian). Behavior is not defined when `value` is anything other than a 32-bit
float.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Example:

```js
const buf = new Buffer(4);
buf.writeFloatBE(0xcafebabe, 0);

console.log(buf);
  // Prints: <Buffer 4f 4a fe bb>

buf.writeFloatLE(0xcafebabe, 0);

console.log(buf);
  // Prints: <Buffer bb fe 4a 4f>
```

### buf.writeInt8(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 1`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset`. The `value` should be a
valid signed 8-bit integer.  Behavior is not defined when `value` is anything
other than a signed 8-bit integer.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

The `value` is interpreted and written as a two's complement signed integer.

```js
const buf = new Buffer(2);
buf.writeInt8(2, 0);
buf.writeInt8(-2, 1);
console.log(buf);
  // Prints: <Buffer 02 fe>
```

### buf.writeInt16BE(value, offset[, noAssert])
### buf.writeInt16LE(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 2`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` with specified endian
format (`writeInt16BE()` writes big endian, `writeInt16LE()` writes little
endian). The `value` should be a valid signed 16-bit integer. Behavior is
not defined when `value` is anything other than a signed 16-bit integer.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

The `value` is interpreted and written as a two's complement signed integer.

```js
const buf = new Buffer(4);
buf.writeInt16BE(0x0102,0);
buf.writeInt16LE(0x0304,2);
console.log(buf);
  // Prints: <Buffer 01 02 04 03>
```

### buf.writeInt32BE(value, offset[, noAssert])
### buf.writeInt32LE(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 4`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` with specified endian
format (`writeInt32BE()` writes big endian, `writeInt32LE()` writes little
endian). The `value` should be a valid signed 32-bit integer. Behavior is
not defined when `value` is anything other than a signed 32-bit integer.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

The `value` is interpreted and written as a two's complement signed integer.

```js
const buf = new Buffer(8);
buf.writeInt32BE(0x01020304,0);
buf.writeInt32LE(0x05060708,4);
console.log(buf);
  // Prints: <Buffer 01 02 03 04 08 07 06 05>
```

### buf.writeIntBE(value, offset, byteLength[, noAssert])
### buf.writeIntLE(value, offset, byteLength[, noAssert])
<!-- YAML
added: v1.0.0
-->

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - byteLength`
* `byteLength` {Number} `0 < byteLength <= 6`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` and `byteLength`.
Supports up to 48 bits of accuracy. For example:

```js
const buf1 = new Buffer(6);
buf1.writeUIntBE(0x1234567890ab, 0, 6);
console.log(buf1);
  // Prints: <Buffer 12 34 56 78 90 ab>

const buf2 = new Buffer(6);
buf2.writeUIntLE(0x1234567890ab, 0, 6);
console.log(buf2);
  // Prints: <Buffer ab 90 78 56 34 12>
```

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Behavior is not defined when `value` is anything other than an integer.

### buf.writeUInt8(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 1`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset`. The `value` should be a
valid unsigned 8-bit integer.  Behavior is not defined when `value` is anything
other than an unsigned 8-bit integer.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Example:

```js
const buf = new Buffer(4);
buf.writeUInt8(0x3, 0);
buf.writeUInt8(0x4, 1);
buf.writeUInt8(0x23, 2);
buf.writeUInt8(0x42, 3);

console.log(buf);
  // Prints: <Buffer 03 04 23 42>
```

### buf.writeUInt16BE(value, offset[, noAssert])
### buf.writeUInt16LE(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 2`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` with specified endian
format (`writeUInt16BE()` writes big endian, `writeUInt16LE()` writes little
endian). The `value` should be a valid unsigned 16-bit integer. Behavior is
not defined when `value` is anything other than an unsigned 16-bit integer.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Example:

```js
const buf = new Buffer(4);
buf.writeUInt16BE(0xdead, 0);
buf.writeUInt16BE(0xbeef, 2);

console.log(buf);
  // Prints: <Buffer de ad be ef>

buf.writeUInt16LE(0xdead, 0);
buf.writeUInt16LE(0xbeef, 2);

console.log(buf);
  // Prints: <Buffer ad de ef be>
```

### buf.writeUInt32BE(value, offset[, noAssert])
### buf.writeUInt32LE(value, offset[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - 4`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` with specified endian
format (`writeUInt32BE()` writes big endian, `writeUInt32LE()` writes little
endian). The `value` should be a valid unsigned 32-bit integer. Behavior is
not defined when `value` is anything other than an unsigned 32-bit integer.

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Example:

```js
const buf = new Buffer(4);
buf.writeUInt32BE(0xfeedface, 0);

console.log(buf);
  // Prints: <Buffer fe ed fa ce>

buf.writeUInt32LE(0xfeedface, 0);

console.log(buf);
  // Prints: <Buffer ce fa ed fe>
```

### buf.writeUIntBE(value, offset, byteLength[, noAssert])
### buf.writeUIntLE(value, offset, byteLength[, noAssert])

* `value` {Number} Bytes to be written to Buffer
* `offset` {Number} `0 <= offset <= buf.length - byteLength`
* `byteLength` {Number} `0 < byteLength <= 6`
* `noAssert` {Boolean} Default: false
* Return: {Number} The offset plus the number of written bytes

Writes `value` to the Buffer at the specified `offset` and `byteLength`.
Supports up to 48 bits of accuracy. For example:

```js
const buf = new Buffer(6);
buf.writeUIntBE(0x1234567890ab, 0, 6);
console.log(buf);
  // Prints: <Buffer 12 34 56 78 90 ab>
```

Set `noAssert` to true to skip validation of `value` and `offset`. This means
that `value` may be too large for the specific function and `offset` may be
beyond the end of the Buffer leading to the values being silently dropped. This
should not be used unless you are certain of correctness.

Behavior is not defined when `value` is anything other than an unsigned integer.

## buffer.INSPECT_MAX_BYTES

* {Number} Default: 50

Returns the maximum number of bytes that will be returned when
`buffer.inspect()` is called. This can be overridden by user modules. See
[`util.inspect()`][] for more details on `buffer.inspect()` behavior.

Note that this is a property on the `buffer` module as returned by
`require('buffer')`, not on the Buffer global or a Buffer instance.

## Class: SlowBuffer

Returns an un-pooled `Buffer`.

In order to avoid the garbage collection overhead of creating many individually
allocated Buffers, by default allocations under 4KB are sliced from a single
larger allocated object. This approach improves both performance and memory
usage since v8 does not need to track and cleanup as many `Persistent` objects.

In the case where a developer may need to retain a small chunk of memory from a
pool for an indeterminate amount of time, it may be appropriate to create an
un-pooled Buffer instance using `SlowBuffer` then copy out the relevant bits.

```js
// need to keep around a few small chunks of memory
const store = [];

socket.on('readable', () => {
  var data = socket.read();
  // allocate for retained data
  var sb = new SlowBuffer(10);
  // copy the data into the new allocation
  data.copy(sb, 0, 0, 10);
  store.push(sb);
});
```

Use of `SlowBuffer` should be used only as a last resort *after* a developer
has observed undue memory retention in their applications.

### new SlowBuffer(size)

* `size` Number

Allocates a new `SlowBuffer` of `size` bytes.  The `size` must be less than
or equal to the value of `require('buffer').kMaxLength` (on 64-bit
architectures, `kMaxLength` is `(2^31)-1`). Otherwise, a [`RangeError`][] is
thrown. If a `size` less than 0 is specified, a zero-length `SlowBuffer` will be
created.

The underlying memory for `SlowBuffer` instances is *not initialized*. The
contents of a newly created `SlowBuffer` are unknown and could contain
sensitive data. Use [`buf.fill(0)`][] to initialize a `SlowBuffer` to zeroes.

```js
const SlowBuffer = require('buffer').SlowBuffer;
const buf = new SlowBuffer(5);
console.log(buf);
  // <Buffer 78 e0 82 02 01>
  // (octets will be different, every time)
buf.fill(0);
console.log(buf);
  // <Buffer 00 00 00 00 00>
```

[`Array#includes()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/includes
[`Array#indexOf()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/indexOf
[`buf.entries()`]: #buffer_buf_entries
[`buf.fill(0)`]: #buffer_buf_fill_value_offset_end
[`buf.keys()`]: #buffer_buf_keys
[`buf.slice()`]: #buffer_buf_slice_start_end
[`buf.values()`]: #buffer_buf_values
[`buf1.compare(buf2)`]: #buffer_buf_compare_otherbuffer
[`JSON.stringify()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/JSON/stringify
[`RangeError`]: errors.html#errors_class_rangeerror
[`String.prototype.length`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/length
[`util.inspect()`]: util.html#util_util_inspect_object_options
[iterator]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols
[RFC 4648, Section 5]: https://tools.ietf.org/html/rfc4648#section-5
[buffer_from_array]: #buffer_class_method_buffer_from_array
[buffer_from_buffer]: #buffer_class_method_buffer_from_buffer
[buffer_from_arraybuf]: #buffer_class_method_buffer_from_arraybuffer
[buffer_from_string]: #buffer_class_method_buffer_from_str_encoding
[buffer_allocunsafe]: #buffer_class_method_buffer_allocunsafe_size
[buffer_allocunsafeslow]: #buffer_class_method_buffer_allocunsafeslow_size
[buffer_alloc]: #buffer_class_method_buffer_alloc_size_fill_encoding
[`TypedArray.from()`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/TypedArray/from
