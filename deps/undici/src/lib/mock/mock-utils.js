'use strict'

const { MockNotMatchedError } = require('./mock-errors')
const {
  kDispatches,
  kMockAgent,
  kOriginalDispatch,
  kOrigin,
  kGetNetConnect
} = require('./mock-symbols')
const { buildURL, nop } = require('../core/util')

function matchValue (match, value) {
  if (typeof match === 'string') {
    return match === value
  }
  if (match instanceof RegExp) {
    return match.test(value)
  }
  if (typeof match === 'function') {
    return match(value) === true
  }
  return false
}

function lowerCaseEntries (headers) {
  return Object.fromEntries(
    Object.entries(headers).map(([headerName, headerValue]) => {
      return [headerName.toLocaleLowerCase(), headerValue]
    })
  )
}

/**
 * @param {import('../../index').Headers|string[]|Record<string, string>} headers
 * @param {string} key
 */
function getHeaderByName (headers, key) {
  if (Array.isArray(headers)) {
    for (let i = 0; i < headers.length; i += 2) {
      if (headers[i].toLocaleLowerCase() === key.toLocaleLowerCase()) {
        return headers[i + 1]
      }
    }

    return undefined
  } else if (typeof headers.get === 'function') {
    return headers.get(key)
  } else {
    return lowerCaseEntries(headers)[key.toLocaleLowerCase()]
  }
}

/** @param {string[]} headers */
function buildHeadersFromArray (headers) { // fetch HeadersList
  const clone = headers.slice()
  const entries = []
  for (let index = 0; index < clone.length; index += 2) {
    entries.push([clone[index], clone[index + 1]])
  }
  return Object.fromEntries(entries)
}

function matchHeaders (mockDispatch, headers) {
  if (typeof mockDispatch.headers === 'function') {
    if (Array.isArray(headers)) { // fetch HeadersList
      headers = buildHeadersFromArray(headers)
    }
    return mockDispatch.headers(headers ? lowerCaseEntries(headers) : {})
  }
  if (typeof mockDispatch.headers === 'undefined') {
    return true
  }
  if (typeof headers !== 'object' || typeof mockDispatch.headers !== 'object') {
    return false
  }

  for (const [matchHeaderName, matchHeaderValue] of Object.entries(mockDispatch.headers)) {
    const headerValue = getHeaderByName(headers, matchHeaderName)

    if (!matchValue(matchHeaderValue, headerValue)) {
      return false
    }
  }
  return true
}

function matchKey (mockDispatch, { path, method, body, headers }) {
  const pathMatch = matchValue(mockDispatch.path, path)
  const methodMatch = matchValue(mockDispatch.method, method)
  const bodyMatch = typeof mockDispatch.body !== 'undefined' ? matchValue(mockDispatch.body, body) : true
  const headersMatch = matchHeaders(mockDispatch, headers)
  return pathMatch && methodMatch && bodyMatch && headersMatch
}

function getResponseData (data) {
  if (Buffer.isBuffer(data)) {
    return data
  } else if (typeof data === 'object') {
    return JSON.stringify(data)
  } else {
    return data.toString()
  }
}

function getMockDispatch (mockDispatches, key) {
  const resolvedPath = key.query ? buildURL(key.path, key.query) : key.path

  // Match path
  let matchedMockDispatches = mockDispatches.filter(({ consumed }) => !consumed).filter(({ path }) => matchValue(path, resolvedPath))
  if (matchedMockDispatches.length === 0) {
    throw new MockNotMatchedError(`Mock dispatch not matched for path '${resolvedPath}'`)
  }

  // Match method
  matchedMockDispatches = matchedMockDispatches.filter(({ method }) => matchValue(method, key.method))
  if (matchedMockDispatches.length === 0) {
    throw new MockNotMatchedError(`Mock dispatch not matched for method '${key.method}'`)
  }

  // Match body
  matchedMockDispatches = matchedMockDispatches.filter(({ body }) => typeof body !== 'undefined' ? matchValue(body, key.body) : true)
  if (matchedMockDispatches.length === 0) {
    throw new MockNotMatchedError(`Mock dispatch not matched for body '${key.body}'`)
  }

  // Match headers
  matchedMockDispatches = matchedMockDispatches.filter((mockDispatch) => matchHeaders(mockDispatch, key.headers))
  if (matchedMockDispatches.length === 0) {
    throw new MockNotMatchedError(`Mock dispatch not matched for headers '${typeof key.headers === 'object' ? JSON.stringify(key.headers) : key.headers}'`)
  }

  return matchedMockDispatches[0]
}

function addMockDispatch (mockDispatches, key, data) {
  const baseData = { timesInvoked: 0, times: 1, persist: false, consumed: false }
  const replyData = typeof data === 'function' ? { callback: data } : { ...data }
  const newMockDispatch = { ...baseData, ...key, pending: true, data: { error: null, ...replyData } }
  mockDispatches.push(newMockDispatch)
  return newMockDispatch
}

function deleteMockDispatch (mockDispatches, key) {
  const index = mockDispatches.findIndex(dispatch => {
    if (!dispatch.consumed) {
      return false
    }
    return matchKey(dispatch, key)
  })
  if (index !== -1) {
    mockDispatches.splice(index, 1)
  }
}

function buildKey (opts) {
  const { path, method, body, headers, query } = opts
  return {
    path,
    method,
    body,
    headers,
    query
  }
}

function generateKeyValues (data) {
  return Object.entries(data).reduce((keyValuePairs, [key, value]) => [...keyValuePairs, key, value], [])
}

/**
 * @see https://developer.mozilla.org/en-US/docs/Web/HTTP/Status
 * @param {number} statusCode
 */
function getStatusText (statusCode) {
  switch (statusCode) {
    case 100: return 'Continue'
    case 101: return 'Switching Protocols'
    case 102: return 'Processing'
    case 103: return 'Early Hints'
    case 200: return 'OK'
    case 201: return 'Created'
    case 202: return 'Accepted'
    case 203: return 'Non-Authoritative Information'
    case 204: return 'No Content'
    case 205: return 'Reset Content'
    case 206: return 'Partial Content'
    case 207: return 'Multi-Status'
    case 208: return 'Already Reported'
    case 226: return 'IM Used'
    case 300: return 'Multiple Choice'
    case 301: return 'Moved Permanently'
    case 302: return 'Found'
    case 303: return 'See Other'
    case 304: return 'Not Modified'
    case 305: return 'Use Proxy'
    case 306: return 'unused'
    case 307: return 'Temporary Redirect'
    case 308: return 'Permanent Redirect'
    case 400: return 'Bad Request'
    case 401: return 'Unauthorized'
    case 402: return 'Payment Required'
    case 403: return 'Forbidden'
    case 404: return 'Not Found'
    case 405: return 'Method Not Allowed'
    case 406: return 'Not Acceptable'
    case 407: return 'Proxy Authentication Required'
    case 408: return 'Request Timeout'
    case 409: return 'Conflict'
    case 410: return 'Gone'
    case 411: return 'Length Required'
    case 412: return 'Precondition Failed'
    case 413: return 'Payload Too Large'
    case 414: return 'URI Too Large'
    case 415: return 'Unsupported Media Type'
    case 416: return 'Range Not Satisfiable'
    case 417: return 'Expectation Failed'
    case 418: return 'I\'m a teapot'
    case 421: return 'Misdirected Request'
    case 422: return 'Unprocessable Entity'
    case 423: return 'Locked'
    case 424: return 'Failed Dependency'
    case 425: return 'Too Early'
    case 426: return 'Upgrade Required'
    case 428: return 'Precondition Required'
    case 429: return 'Too Many Requests'
    case 431: return 'Request Header Fields Too Large'
    case 451: return 'Unavailable For Legal Reasons'
    case 500: return 'Internal Server Error'
    case 501: return 'Not Implemented'
    case 502: return 'Bad Gateway'
    case 503: return 'Service Unavailable'
    case 504: return 'Gateway Timeout'
    case 505: return 'HTTP Version Not Supported'
    case 506: return 'Variant Also Negotiates'
    case 507: return 'Insufficient Storage'
    case 508: return 'Loop Detected'
    case 510: return 'Not Extended'
    case 511: return 'Network Authentication Required'
    default: return 'unknown'
  }
}

async function getResponse (body) {
  const buffers = []
  for await (const data of body) {
    buffers.push(data)
  }
  return Buffer.concat(buffers).toString('utf8')
}

/**
 * Mock dispatch function used to simulate undici dispatches
 */
function mockDispatch (opts, handler) {
  // Get mock dispatch from built key
  const key = buildKey(opts)
  const mockDispatch = getMockDispatch(this[kDispatches], key)

  mockDispatch.timesInvoked++

  // Here's where we resolve a callback if a callback is present for the dispatch data.
  if (mockDispatch.data.callback) {
    mockDispatch.data = { ...mockDispatch.data, ...mockDispatch.data.callback(opts) }
  }

  // Parse mockDispatch data
  const { data: { statusCode, data, headers, trailers, error }, delay, persist } = mockDispatch
  const { timesInvoked, times } = mockDispatch

  // If it's used up and not persistent, mark as consumed
  mockDispatch.consumed = !persist && timesInvoked >= times
  mockDispatch.pending = timesInvoked < times

  // If specified, trigger dispatch error
  if (error !== null) {
    deleteMockDispatch(this[kDispatches], key)
    handler.onError(error)
    return true
  }

  // Handle the request with a delay if necessary
  if (typeof delay === 'number' && delay > 0) {
    setTimeout(() => {
      handleReply(this[kDispatches])
    }, delay)
  } else {
    handleReply(this[kDispatches])
  }

  function handleReply (mockDispatches) {
    // fetch's HeadersList is a 1D string array
    const optsHeaders = Array.isArray(opts.headers)
      ? buildHeadersFromArray(opts.headers)
      : opts.headers
    const responseData = getResponseData(
      typeof data === 'function' ? data({ ...opts, headers: optsHeaders }) : data
    )
    const responseHeaders = generateKeyValues(headers)
    const responseTrailers = generateKeyValues(trailers)

    handler.abort = nop
    handler.onHeaders(statusCode, responseHeaders, resume, getStatusText(statusCode))
    handler.onData(Buffer.from(responseData))
    handler.onComplete(responseTrailers)
    deleteMockDispatch(mockDispatches, key)
  }

  function resume () {}

  return true
}

function buildMockDispatch () {
  const agent = this[kMockAgent]
  const origin = this[kOrigin]
  const originalDispatch = this[kOriginalDispatch]

  return function dispatch (opts, handler) {
    if (agent.isMockActive) {
      try {
        mockDispatch.call(this, opts, handler)
      } catch (error) {
        if (error instanceof MockNotMatchedError) {
          const netConnect = agent[kGetNetConnect]()
          if (netConnect === false) {
            throw new MockNotMatchedError(`${error.message}: subsequent request to origin ${origin} was not allowed (net.connect disabled)`)
          }
          if (checkNetConnect(netConnect, origin)) {
            originalDispatch.call(this, opts, handler)
          } else {
            throw new MockNotMatchedError(`${error.message}: subsequent request to origin ${origin} was not allowed (net.connect is not enabled for this origin)`)
          }
        } else {
          throw error
        }
      }
    } else {
      originalDispatch.call(this, opts, handler)
    }
  }
}

function checkNetConnect (netConnect, origin) {
  const url = new URL(origin)
  if (netConnect === true) {
    return true
  } else if (Array.isArray(netConnect) && netConnect.some((matcher) => matchValue(matcher, url.host))) {
    return true
  }
  return false
}

function buildMockOptions (opts) {
  if (opts) {
    const { agent, ...mockOptions } = opts
    return mockOptions
  }
}

module.exports = {
  getResponseData,
  getMockDispatch,
  addMockDispatch,
  deleteMockDispatch,
  buildKey,
  generateKeyValues,
  matchValue,
  getResponse,
  getStatusText,
  mockDispatch,
  buildMockDispatch,
  checkNetConnect,
  buildMockOptions,
  getHeaderByName
}
