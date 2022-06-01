'use strict'

const corsSafeListedMethods = ['GET', 'HEAD', 'POST']

const nullBodyStatus = [101, 204, 205, 304]

const redirectStatus = [301, 302, 303, 307, 308]

const referrerPolicy = [
  '',
  'no-referrer',
  'no-referrer-when-downgrade',
  'same-origin',
  'origin',
  'strict-origin',
  'origin-when-cross-origin',
  'strict-origin-when-cross-origin',
  'unsafe-url'
]

const requestRedirect = ['follow', 'manual', 'error']

const safeMethods = ['GET', 'HEAD', 'OPTIONS', 'TRACE']

const requestMode = ['navigate', 'same-origin', 'no-cors', 'cors']

const requestCredentials = ['omit', 'same-origin', 'include']

const requestCache = [
  'default',
  'no-store',
  'reload',
  'no-cache',
  'force-cache',
  'only-if-cached'
]

const requestBodyHeader = [
  'content-encoding',
  'content-language',
  'content-location',
  'content-type'
]

// http://fetch.spec.whatwg.org/#forbidden-method
const forbiddenMethods = ['CONNECT', 'TRACE', 'TRACK']

const subresource = [
  'audio',
  'audioworklet',
  'font',
  'image',
  'manifest',
  'paintworklet',
  'script',
  'style',
  'track',
  'video',
  'xslt',
  ''
]

module.exports = {
  subresource,
  forbiddenMethods,
  requestBodyHeader,
  referrerPolicy,
  requestRedirect,
  requestMode,
  requestCredentials,
  requestCache,
  redirectStatus,
  corsSafeListedMethods,
  nullBodyStatus,
  safeMethods
}
