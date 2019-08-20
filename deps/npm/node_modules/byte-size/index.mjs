/**
 * An isomorphic, load-anywhere function to convert a bytes value into a more human-readable format. Choose between [metric or IEC units](https://en.wikipedia.org/wiki/Gigabyte), summarised below.
 *
 * Value | Metric
 * ----- | -------------
 * 1000  | kB  kilobyte
 * 1000^2 | MB  megabyte
 * 1000^3 | GB  gigabyte
 * 1000^4 | TB  terabyte
 * 1000^5 | PB  petabyte
 * 1000^6 | EB  exabyte
 * 1000^7 | ZB  zettabyte
 * 1000^8 | YB  yottabyte
 *
 * Value | IEC
 * ----- | ------------
 * 1024  | KiB kibibyte
 * 1024^2 | MiB mebibyte
 * 1024^3 | GiB gibibyte
 * 1024^4 | TiB tebibyte
 * 1024^5 | PiB pebibyte
 * 1024^6 | EiB exbibyte
 * 1024^7 | ZiB zebibyte
 * 1024^8 | YiB yobibyte
 *
 * Value | Metric (octet)
 * ----- | -------------
 * 1000  | ko  kilooctet
 * 1000^2 | Mo  megaoctet
 * 1000^3 | Go  gigaoctet
 * 1000^4 | To  teraoctet
 * 1000^5 | Po  petaoctet
 * 1000^6 | Eo  exaoctet
 * 1000^7 | Zo  zettaoctet
 * 1000^8 | Yo  yottaoctet
 *
 * Value | IEC (octet)
 * ----- | ------------
 * 1024  | Kio kilooctet
 * 1024^2 | Mio mebioctet
 * 1024^3 | Gio gibioctet
 * 1024^4 | Tio tebioctet
 * 1024^5 | Pio pebioctet
 * 1024^6 | Eio exbioctet
 * 1024^7 | Zio zebioctet
 * 1024^8 | Yio yobioctet
 *
 * @module byte-size
 * @example
 * ```js
 * const byteSize = require('byte-size')
 * ```
 */

class ByteSize {
  constructor (bytes, options) {
    options = options || {}
    options.units = options.units || 'metric'
    options.precision = typeof options.precision === 'undefined' ? 1 : options.precision

    const table = [
      { expFrom: 0, expTo: 1, metric: 'B', iec: 'B', metric_octet: 'o', iec_octet: 'o' },
      { expFrom: 1, expTo: 2, metric: 'kB', iec: 'KiB', metric_octet: 'ko', iec_octet: 'Kio' },
      { expFrom: 2, expTo: 3, metric: 'MB', iec: 'MiB', metric_octet: 'Mo', iec_octet: 'Mio' },
      { expFrom: 3, expTo: 4, metric: 'GB', iec: 'GiB', metric_octet: 'Go', iec_octet: 'Gio' },
      { expFrom: 4, expTo: 5, metric: 'TB', iec: 'TiB', metric_octet: 'To', iec_octet: 'Tio' },
      { expFrom: 5, expTo: 6, metric: 'PB', iec: 'PiB', metric_octet: 'Po', iec_octet: 'Pio' },
      { expFrom: 6, expTo: 7, metric: 'EB', iec: 'EiB', metric_octet: 'Eo', iec_octet: 'Eio' },
      { expFrom: 7, expTo: 8, metric: 'ZB', iec: 'ZiB', metric_octet: 'Zo', iec_octet: 'Zio' },
      { expFrom: 8, expTo: 9, metric: 'YB', iec: 'YiB', metric_octet: 'Yo', iec_octet: 'Yio' }
    ]

    const base = options.units === 'metric' || options.units === 'metric_octet' ? 1000 : 1024
    const prefix = bytes < 0 ? '-' : '';
    bytes = Math.abs(bytes);

    for (let i = 0; i < table.length; i++) {
      const lower = Math.pow(base, table[i].expFrom)
      const upper = Math.pow(base, table[i].expTo)
      if (bytes >= lower && bytes < upper) {
        const units = table[i][options.units]
        if (i === 0) {
          this.value = prefix + bytes
          this.unit = units
          return
        } else {
          this.value = prefix + (bytes / lower).toFixed(options.precision)
          this.unit = units
          return
        }
      }
    }

    this.value = prefix + bytes
    this.unit = ''
  }

  toString () {
    return `${this.value} ${this.unit}`.trim()
  }
}

/**
 * @param {number} - the bytes value to convert.
 * @param [options] {object} - optional config.
 * @param [options.precision=1] {number} - number of decimal places.
 * @param [options.units=metric] {string} - select `'metric'`, `'iec'`, `'metric_octet'` or `'iec_octet'` units.
 * @returns {{ value: string, unit: string}}
 * @alias module:byte-size
 * @example
 * ```js
 * > const byteSize = require('byte-size')
 *
 * > byteSize(1580)
 * { value: '1.6', unit: 'kB' }
 *
 * > byteSize(1580, { units: 'iec' })
 * { value: '1.5', unit: 'KiB' }
 *
 * > byteSize(1580, { units: 'iec', precision: 3 })
 * { value: '1.543', unit: 'KiB' }
 *
 * > byteSize(1580, { units: 'iec', precision: 0 })
 * { value: '2', unit: 'KiB' }
 *
 * > byteSize(1580, { units: 'metric_octet' })
 * { value: '1.6', unit: 'ko' }
 *
 * > byteSize(1580, { units: 'iec_octet' })
 * { value: '1.5', unit: 'Kio' }
 *
 * > byteSize(1580, { units: 'iec_octet' }).toString()
 * '1.5 Kio'
 *
 * > const { value, unit }  = byteSize(1580, { units: 'iec_octet' })
 * > `${value} ${unit}`
 * '1.5 Kio'
 * ```
 */
function byteSize (bytes, options) {
  return new ByteSize(bytes, options)
}

export default byteSize
