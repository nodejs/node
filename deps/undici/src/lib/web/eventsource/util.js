'use strict'

const { makeRequest } = require('../fetch/request')

/**
 * Checks if the given value is a valid LastEventId.
 * @param {string} value
 * @returns {boolean}
 */
function isValidLastEventId (value) {
  // LastEventId should not contain U+0000 NULL
  return value.indexOf('\u0000') === -1
}

/**
 * Checks if the given value is a base 10 digit.
 * @param {string} value
 * @returns {boolean}
 */
function isASCIINumber (value) {
  if (value.length === 0) return false
  for (let i = 0; i < value.length; i++) {
    if (value.charCodeAt(i) < 0x30 || value.charCodeAt(i) > 0x39) return false
  }
  return true
}

function createPotentialCORSRequest (url, destination, corsAttributeState, sameOriginFallback) {
  // 1. Let mode be "no-cors" if corsAttributeState is No CORS, and "cors" otherwise.
  let mode = corsAttributeState === 'no cors' ? 'no-cors' : 'cors'

  // 2. If same-origin fallback flag is set and mode is "no-cors", set mode to "same-origin".
  if (sameOriginFallback && mode === 'no-cors') {
    mode = 'same-origin'
  }

  // 3. Let credentialsMode be "include".
  let credentialsMode = 'include'

  // 4. If corsAttributeState is Anonymous, set credentialsMode to "same-origin".
  if (corsAttributeState === 'anonymous') {
    credentialsMode = 'same-origin'
  }

  // 5. Return a new request whose URL is url, destination is destination, mode is mode,
  //    credentials mode is credentialsMode, and whose use-URL-credentials flag is set.
  return makeRequest({
    urlList: [url],
    destination,
    mode,
    credentials: credentialsMode,
    useCredentials: true
  })
}

module.exports = {
  isValidLastEventId,
  isASCIINumber,
  createPotentialCORSRequest
}
