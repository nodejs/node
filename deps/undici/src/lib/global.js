'use strict'

// We include a version number for the Dispatcher API. In case of breaking changes,
// this version number must be increased to avoid conflicts.
const globalDispatcher = Symbol.for('undici.globalDispatcher.2')
const legacyGlobalDispatcher = Symbol.for('undici.globalDispatcher.1')
const { InvalidArgumentError } = require('./core/errors')
const Agent = require('./dispatcher/agent')
const Dispatcher1Wrapper = require('./dispatcher/dispatcher1-wrapper')

const nodeMajor = Number(process.versions.node.split('.', 1)[0])

if (getGlobalDispatcher() === undefined) {
  setGlobalDispatcher(new Agent())
}

function setGlobalDispatcher (agent) {
  if (!agent || typeof agent.dispatch !== 'function') {
    throw new InvalidArgumentError('Argument agent must implement Agent')
  }

  Object.defineProperty(globalThis, globalDispatcher, {
    value: agent,
    writable: true,
    enumerable: false,
    configurable: false
  })

  if (nodeMajor === 22) {
    const legacyAgent = agent instanceof Dispatcher1Wrapper ? agent : new Dispatcher1Wrapper(agent)

    Object.defineProperty(globalThis, legacyGlobalDispatcher, {
      value: legacyAgent,
      writable: true,
      enumerable: false,
      configurable: false
    })
  }
}

function getGlobalDispatcher () {
  return globalThis[globalDispatcher]
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
