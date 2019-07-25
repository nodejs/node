[![view on npm](https://img.shields.io/npm/v/byte-size.svg)](https://www.npmjs.org/package/byte-size)
[![npm module downloads](https://img.shields.io/npm/dt/byte-size.svg)](https://www.npmjs.org/package/byte-size)
[![Build Status](https://travis-ci.org/75lb/byte-size.svg?branch=master)](https://travis-ci.org/75lb/byte-size)
[![Coverage Status](https://coveralls.io/repos/github/75lb/byte-size/badge.svg?branch=master)](https://coveralls.io/github/75lb/byte-size?branch=master)
[![Dependency Status](https://david-dm.org/75lb/byte-size.svg)](https://david-dm.org/75lb/byte-size)
[![js-standard-style](https://img.shields.io/badge/code%20style-standard-brightgreen.svg)](https://github.com/feross/standard)

<a name="module_byte-size"></a>

## byte-size
An isomorphic, load-anywhere function to convert a bytes value into a more human-readable format. Choose between [metric or IEC units](https://en.wikipedia.org/wiki/Gigabyte), summarised below.

Value | Metric
----- | -------------
1000  | kB  kilobyte
1000^2 | MB  megabyte
1000^3 | GB  gigabyte
1000^4 | TB  terabyte
1000^5 | PB  petabyte
1000^6 | EB  exabyte
1000^7 | ZB  zettabyte
1000^8 | YB  yottabyte

Value | IEC
----- | ------------
1024  | KiB kibibyte
1024^2 | MiB mebibyte
1024^3 | GiB gibibyte
1024^4 | TiB tebibyte
1024^5 | PiB pebibyte
1024^6 | EiB exbibyte
1024^7 | ZiB zebibyte
1024^8 | YiB yobibyte

Value | Metric (octet)
----- | -------------
1000  | ko  kilooctet
1000^2 | Mo  megaoctet
1000^3 | Go  gigaoctet
1000^4 | To  teraoctet
1000^5 | Po  petaoctet
1000^6 | Eo  exaoctet
1000^7 | Zo  zettaoctet
1000^8 | Yo  yottaoctet

Value | IEC (octet)
----- | ------------
1024  | Kio kilooctet
1024^2 | Mio mebioctet
1024^3 | Gio gibioctet
1024^4 | Tio tebioctet
1024^5 | Pio pebioctet
1024^6 | Eio exbioctet
1024^7 | Zio zebioctet
1024^8 | Yio yobioctet

**Example**  
```js
const byteSize = require('byte-size')
```
<a name="exp_module_byte-size--byteSize"></a>

### byteSize(bytes, [options]) ⇒ <code>Object</code> ⏏
**Kind**: Exported function  

| Param | Type | Default | Description |
| --- | --- | --- | --- |
| bytes | <code>number</code> |  | the bytes value to convert. |
| [options] | <code>object</code> |  | optional config. |
| [options.precision] | <code>number</code> | <code>1</code> | number of decimal places. |
| [options.units] | <code>string</code> | <code>&quot;metric&quot;</code> | select `'metric'`, `'iec'`, `'metric_octet'` or `'iec_octet'` units. |

**Example**  
```js
> const byteSize = require('byte-size')

> byteSize(1580)
{ value: '1.6', unit: 'kB' }

> byteSize(1580, { units: 'iec' })
{ value: '1.5', unit: 'KiB' }

> byteSize(1580, { units: 'iec', precision: 3 })
{ value: '1.543', unit: 'KiB' }

> byteSize(1580, { units: 'iec', precision: 0 })
{ value: '2', unit: 'KiB' }

> byteSize(1580, { units: 'metric_octet' })
{ value: '1.6', unit: 'ko' }

> byteSize(1580, { units: 'iec_octet' })
{ value: '1.5', unit: 'Kio' }

> byteSize(1580, { units: 'iec_octet' }).toString()
'1.5 Kio'

> const { value, unit }  = byteSize(1580, { units: 'iec_octet' })
> `${value} ${unit}`
'1.5 Kio'
```

### Load anywhere

This library is compatible with Node.js, the Web and any style of module loader. It can be loaded anywhere, natively without transpilation.

Node.js:

```js
const byteSize = require('byte-size')
```

Within Node.js with ECMAScript Module support enabled:

```js
import byteSize from 'byte-size'
```

Within a modern browser ECMAScript Module:

```js
import byteSize from './node_modules/byte-size/index.mjs'
```

Old browser (adds `window.byteSize`):

```html
<script nomodule src="./node_modules/byte-size/dist/index.js"></script>
```

* * *

&copy; 2014-18 Lloyd Brookes \<75pound@gmail.com\>. Documented by [jsdoc-to-markdown](https://github.com/jsdoc2md/jsdoc-to-markdown).
