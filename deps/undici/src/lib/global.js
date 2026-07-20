'use strict'

// We include a version number for the Dispatcher API. In case of breaking changes,
// this version number must be increased to avoid conflicts.
const globalDispatcher = Symbol.for('undici.globalDispatcher.2')
const legacyGlobalDispatcher = Symbol.for('undici.globalDispatcher.1')
const { InvalidArgumentError } = require('./core/errors')
const Agent = require('./dispatcher/agent')
const Dispatcher1Wrapper = require('./dispatcher/dispatcher1-wrapper')

// Fallback storage used when globalThis is not extensible (e.g. Object.freeze(globalThis)).
let fallbackDispatcher

if (getGlobalDispatcher() === undefined) {
  setGlobalDispatcher(new Agent())
}

function setGlobalDispatcher (agent) {
  if (!agent || typeof agent.dispatch !== 'function') {
    throw new InvalidArgumentError('Argument agent must implement Agent')
  }

  try {
    Object.defineProperty(globalThis, globalDispatcher, {
      value: agent,
      writable: true,
      enumerable: false,
      configurable: false
    })

    const legacyAgent = agent instanceof Dispatcher1Wrapper ? agent : new Dispatcher1Wrapper(agent)

    Object.defineProperty(globalThis, legacyGlobalDispatcher, {
      value: legacyAgent,
      writable: true,
      enumerable: false,
      configurable: false
    })
  } catch {
    // globalThis is not extensible (e.g. frozen via Object.freeze(globalThis)).
    // Fall back to module-level storage so that fetch, WebSocket, etc. still work.
    fallbackDispatcher = agent
  }
}

function getGlobalDispatcher () {
  return globalThis[globalDispatcher] ?? fallbackDispatcher
}

// These are the globals that can be installed by undici.install().
// Not exported by index.js to avoid use outside of this module.
const installedExports = /** @type {const} */ (
  [
    'fetch',
    'Headers',
    'Response',
    'Request',
    'FormData',
    'WebSocket',
    'CloseEvent',
    'ErrorEvent',
    'MessageEvent',
    'EventSource'
  ]
)

module.exports = {
  setGlobalDispatcher,
  getGlobalDispatcher,
  installedExports
}
