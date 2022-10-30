'use strict'

const net = require('net')
const assert = require('assert')
const util = require('./util')
const { InvalidArgumentError, ConnectTimeoutError } = require('./errors')
let tls // include tls conditionally since it is not always available

// TODO: session re-use does not wait for the first
// connection to resolve the session and might therefore
// resolve the same servername multiple times even when
// re-use is enabled.

function buildConnector ({ maxCachedSessions, socketPath, timeout, ...opts }) {
  if (maxCachedSessions != null && (!Number.isInteger(maxCachedSessions) || maxCachedSessions < 0)) {
    throw new InvalidArgumentError('maxCachedSessions must be a positive integer or zero')
  }

  const options = { path: socketPath, ...opts }
  const sessionCache = new Map()
  timeout = timeout == null ? 10e3 : timeout
  maxCachedSessions = maxCachedSessions == null ? 100 : maxCachedSessions

  return function connect ({ hostname, host, protocol, port, servername, localAddress, httpSocket }, callback) {
    let socket
    if (protocol === 'https:') {
      if (!tls) {
        tls = require('tls')
      }
      servername = servername || options.servername || util.getServerName(host) || null

      const sessionKey = servername || hostname
      const session = sessionCache.get(sessionKey) || null

      assert(sessionKey)

      socket = tls.connect({
        highWaterMark: 16384, // TLS in node can't have bigger HWM anyway...
        ...options,
        servername,
        session,
        localAddress,
        socket: httpSocket, // upgrade socket connection
        port: port || 443,
        host: hostname
      })

      socket
        .on('session', function (session) {
          // cache is disabled
          if (maxCachedSessions === 0) {
            return
          }

          if (sessionCache.size >= maxCachedSessions) {
            // remove the oldest session
            const { value: oldestKey } = sessionCache.keys().next()
            sessionCache.delete(oldestKey)
          }

          sessionCache.set(sessionKey, session)
        })
        .on('error', function (err) {
          if (sessionKey && err.code !== 'UND_ERR_INFO') {
            // TODO (fix): Only delete for session related errors.
            sessionCache.delete(sessionKey)
          }
        })
    } else {
      assert(!httpSocket, 'httpSocket can only be sent on TLS update')
      socket = net.connect({
        highWaterMark: 64 * 1024, // Same as nodejs fs streams.
        ...options,
        localAddress,
        port: port || 80,
        host: hostname
      })
    }

    const cancelTimeout = setupTimeout(() => onConnectTimeout(socket), timeout)

    socket
      .setNoDelay(true)
      .once(protocol === 'https:' ? 'secureConnect' : 'connect', function () {
        cancelTimeout()

        if (callback) {
          const cb = callback
          callback = null
          cb(null, this)
        }
      })
      .on('error', function (err) {
        cancelTimeout()

        if (callback) {
          const cb = callback
          callback = null
          cb(err)
        }
      })

    return socket
  }
}

function setupTimeout (onConnectTimeout, timeout) {
  if (!timeout) {
    return () => {}
  }

  let s1 = null
  let s2 = null
  const timeoutId = setTimeout(() => {
    // setImmediate is added to make sure that we priotorise socket error events over timeouts
    s1 = setImmediate(() => {
      if (process.platform === 'win32') {
        // Windows needs an extra setImmediate probably due to implementation differences in the socket logic
        s2 = setImmediate(() => onConnectTimeout())
      } else {
        onConnectTimeout()
      }
    })
  }, timeout)
  return () => {
    clearTimeout(timeoutId)
    clearImmediate(s1)
    clearImmediate(s2)
  }
}

function onConnectTimeout (socket) {
  util.destroy(socket, new ConnectTimeoutError())
}

module.exports = buildConnector
