'use strict'

/**
 * @param {string} value
 * @returns {boolean}
 */
function isCTLExcludingHtab (value) {
  for (let i = 0; i < value.length; ++i) {
    const code = value.charCodeAt(i)

    if (
      (code >= 0x00 && code <= 0x08) ||
      (code >= 0x0A && code <= 0x1F) ||
      code === 0x7F
    ) {
      return true
    }
  }
  return false
}

/**
 CHAR           = <any US-ASCII character (octets 0 - 127)>
 token          = 1*<any CHAR except CTLs or separators>
 separators     = "(" | ")" | "<" | ">" | "@"
                | "," | ";" | ":" | "\" | <">
                | "/" | "[" | "]" | "?" | "="
                | "{" | "}" | SP | HT
 * @param {string} name
 */
function validateCookieName (name) {
  for (let i = 0; i < name.length; ++i) {
    const code = name.charCodeAt(i)

    if (
      code < 0x21 || // exclude CTLs (0-31), SP and HT
      code > 0x7E || // exclude non-ascii and DEL
      code === 0x22 || // "
      code === 0x28 || // (
      code === 0x29 || // )
      code === 0x3C || // <
      code === 0x3E || // >
      code === 0x40 || // @
      code === 0x2C || // ,
      code === 0x3B || // ;
      code === 0x3A || // :
      code === 0x5C || // \
      code === 0x2F || // /
      code === 0x5B || // [
      code === 0x5D || // ]
      code === 0x3F || // ?
      code === 0x3D || // =
      code === 0x7B || // {
      code === 0x7D // }
    ) {
      throw new Error('Invalid cookie name')
    }
  }
}

/**
 cookie-value      = *cookie-octet / ( DQUOTE *cookie-octet DQUOTE )
 cookie-octet      = %x21 / %x23-2B / %x2D-3A / %x3C-5B / %x5D-7E
                       ; US-ASCII characters excluding CTLs,
                       ; whitespace DQUOTE, comma, semicolon,
                       ; and backslash
 * @param {string} value
 */
function validateCookieValue (value) {
  let len = value.length
  let i = 0

  // if the value is wrapped in DQUOTE
  if (value[0] === '"') {
    if (len === 1 || value[len - 1] !== '"') {
      throw new Error('Invalid cookie value')
    }
    --len
    ++i
  }

  while (i < len) {
    const code = value.charCodeAt(i++)

    if (
      code < 0x21 || // exclude CTLs (0-31)
      code > 0x7E || // non-ascii and DEL (127)
      code === 0x22 || // "
      code === 0x2C || // ,
      code === 0x3B || // ;
      code === 0x5C // \
    ) {
      throw new Error('Invalid cookie value')
    }
  }
}

/**
 * path-value        = <any CHAR except CTLs or ";">
 * @param {string} path
 */
function validateCookiePath (path) {
  for (let i = 0; i < path.length; ++i) {
    const code = path.charCodeAt(i)

    if (
      code < 0x20 || // exclude CTLs (0-31)
      code === 0x7F || // DEL
      code === 0x3B // ;
    ) {
      throw new Error('Invalid cookie path')
    }
  }
}

/**
 * I have no idea why these values aren't allowed to be honest,
 * but Deno tests these. - Khafra
 * @param {string} domain
 */
function validateCookieDomain (domain) {
  if (
    domain.startsWith('-') ||
    domain.endsWith('.') ||
    domain.endsWith('-')
  ) {
    throw new Error('Invalid cookie domain')
  }
}

const IMFDays = [
  'Sun', 'Mon', 'Tue', 'Wed',
  'Thu', 'Fri', 'Sat'
]

const IMFMonths = [
  'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
  'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'
]

const IMFPaddedNumbers = Array(61).fill(0).map((_, i) => i.toString().padStart(2, '0'))

/**
 * @see https://www.rfc-editor.org/rfc/rfc7231#section-7.1.1.1
 * @param {number|Date} date
  IMF-fixdate  = day-name "," SP date1 SP time-of-day SP GMT
  ; fixed length/zone/capitalization subset of the format
  ; see Section 3.3 of [RFC5322]

  day-name     = %x4D.6F.6E ; "Mon", case-sensitive
              / %x54.75.65 ; "Tue", case-sensitive
              / %x57.65.64 ; "Wed", case-sensitive
              / %x54.68.75 ; "Thu", case-sensitive
              / %x46.72.69 ; "Fri", case-sensitive
              / %x53.61.74 ; "Sat", case-sensitive
              / %x53.75.6E ; "Sun", case-sensitive
  date1        = day SP month SP year
                  ; e.g., 02 Jun 1982

  day          = 2DIGIT
  month        = %x4A.61.6E ; "Jan", case-sensitive
              / %x46.65.62 ; "Feb", case-sensitive
              / %x4D.61.72 ; "Mar", case-sensitive
              / %x41.70.72 ; "Apr", case-sensitive
              / %x4D.61.79 ; "May", case-sensitive
              / %x4A.75.6E ; "Jun", case-sensitive
              / %x4A.75.6C ; "Jul", case-sensitive
              / %x41.75.67 ; "Aug", case-sensitive
              / %x53.65.70 ; "Sep", case-sensitive
              / %x4F.63.74 ; "Oct", case-sensitive
              / %x4E.6F.76 ; "Nov", case-sensitive
              / %x44.65.63 ; "Dec", case-sensitive
  year         = 4DIGIT

  GMT          = %x47.4D.54 ; "GMT", case-sensitive

  time-of-day  = hour ":" minute ":" second
              ; 00:00:00 - 23:59:60 (leap second)

  hour         = 2DIGIT
  minute       = 2DIGIT
  second       = 2DIGIT
 */
function toIMFDate (date) {
  if (typeof date === 'number') {
    date = new Date(date)
  }

  return `${IMFDays[date.getUTCDay()]}, ${IMFPaddedNumbers[date.getUTCDate()]} ${IMFMonths[date.getUTCMonth()]} ${date.getUTCFullYear()} ${IMFPaddedNumbers[date.getUTCHours()]}:${IMFPaddedNumbers[date.getUTCMinutes()]}:${IMFPaddedNumbers[date.getUTCSeconds()]} GMT`
}

/**
 max-age-av        = "Max-Age=" non-zero-digit *DIGIT
                       ; In practice, both expires-av and max-age-av
                       ; are limited to dates representable by the
                       ; user agent.
 * @param {number} maxAge
 */
function validateCookieMaxAge (maxAge) {
  if (maxAge < 0) {
    throw new Error('Invalid cookie max-age')
  }
}

/**
 * @see https://www.rfc-editor.org/rfc/rfc6265#section-4.1.1
 * @param {import('./index').Cookie} cookie
 */
function stringify (cookie) {
  if (cookie.name.length === 0) {
    return null
  }

  validateCookieName(cookie.name)
  validateCookieValue(cookie.value)

  const out = [`${cookie.name}=${cookie.value}`]

  // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-cookie-prefixes-00#section-3.1
  // https://datatracker.ietf.org/doc/html/draft-ietf-httpbis-cookie-prefixes-00#section-3.2
  if (cookie.name.startsWith('__Secure-')) {
    cookie.secure = true
  }

  if (cookie.name.startsWith('__Host-')) {
    cookie.secure = true
    cookie.domain = null
    cookie.path = '/'
  }

  if (cookie.secure) {
    out.push('Secure')
  }

  if (cookie.httpOnly) {
    out.push('HttpOnly')
  }

  if (typeof cookie.maxAge === 'number') {
    validateCookieMaxAge(cookie.maxAge)
    out.push(`Max-Age=${cookie.maxAge}`)
  }

  if (cookie.domain) {
    validateCookieDomain(cookie.domain)
    out.push(`Domain=${cookie.domain}`)
  }

  if (cookie.path) {
    validateCookiePath(cookie.path)
    out.push(`Path=${cookie.path}`)
  }

  if (cookie.expires && cookie.expires.toString() !== 'Invalid Date') {
    out.push(`Expires=${toIMFDate(cookie.expires)}`)
  }

  if (cookie.sameSite) {
    out.push(`SameSite=${cookie.sameSite}`)
  }

  for (const part of cookie.unparsed) {
    if (!part.includes('=')) {
      throw new Error('Invalid unparsed')
    }

    const [key, ...value] = part.split('=')

    out.push(`${key.trim()}=${value.join('=')}`)
  }

  return out.join('; ')
}

module.exports = {
  isCTLExcludingHtab,
  validateCookieName,
  validateCookiePath,
  validateCookieValue,
  toIMFDate,
  stringify
}
