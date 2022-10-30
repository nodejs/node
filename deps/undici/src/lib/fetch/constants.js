'use strict'

const { MessageChannel, receiveMessageOnPort } = require('worker_threads')

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

/** @type {globalThis['DOMException']} */
const DOMException = globalThis.DOMException ?? (() => {
  // DOMException was only made a global in Node v17.0.0,
  // but fetch supports >= v16.8.
  try {
    atob('~')
  } catch (err) {
    return Object.getPrototypeOf(err).constructor
  }
})()

let channel

/** @type {globalThis['structuredClone']} */
const structuredClone =
  globalThis.structuredClone ??
  // https://github.com/nodejs/node/blob/b27ae24dcc4251bad726d9d84baf678d1f707fed/lib/internal/structured_clone.js
  // structuredClone was added in v17.0.0, but fetch supports v16.8
  function structuredClone (value, options = undefined) {
    if (arguments.length === 0) {
      throw new TypeError('missing argument')
    }

    if (!channel) {
      channel = new MessageChannel()
    }
    channel.port1.unref()
    channel.port2.unref()
    channel.port1.postMessage(value, options?.transfer)
    return receiveMessageOnPort(channel.port2).message
  }

module.exports = {
  DOMException,
  structuredClone,
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
