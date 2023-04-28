
const NPMLOG = require('npmlog')
const { LEVELS } = require('proc-log')

const npmEmitLog = NPMLOG.emitLog.bind(NPMLOG)
const npmLog = NPMLOG.log.bind(NPMLOG)

const merge = (...objs) => objs.reduce((acc, obj) => ({ ...acc, ...obj }))

const mockLogs = (otherMocks = {}) => {
  // Return mocks as an array with getters for each level
  // that return an array of logged properties with the
  // level removed. This is for convenience throughout tests
  const logs = Object.defineProperties(
    [],
    ['timing', ...LEVELS].reduce((acc, level) => {
      acc[level] = {
        get () {
          return this
            .filter(([l]) => level === l)
            .map(([l, ...args]) => args)
        },
      }
      return acc
    }, {})
  )

  // the above logs array is anything logged and it not filtered by level.
  // this display array is filtered and will not include items that
  // would not be shown in the terminal
  const display = Object.defineProperties(
    [],
    ['timing', ...LEVELS].reduce((acc, level) => {
      acc[level] = {
        get () {
          return this
            .filter(([l]) => level === l)
            .map(([l, ...args]) => args)
        },
      }
      return acc
    }, {})
  )

  const npmLogBuffer = []

  // This returns an object with mocked versions of all necessary
  // logging modules. It mocks them with methods that add logs
  // to an array which it also returns. The reason it also returns
  // the mocks is that in tests the same instance of these mocks
  // should be passed to multiple calls to t.mock.
  // XXX: this is messy and fragile and should be removed in favor
  // of some other way to collect and filter logs across all tests
  const logMocks = {
    'proc-log': merge(
      { LEVELS },
      LEVELS.reduce((acc, l) => {
        acc[l] = (...args) => {
          // Re-emit log item for since the log file listens on these
          process.emit('log', l, ...args)
          // Dont add pause/resume events to the logs. Those aren't displayed
          // and emitting them is tested in the display layer
          if (l !== 'pause' && l !== 'resume') {
            logs.push([l, ...args])
          }
        }
        return acc
      }, {}),
      otherMocks['proc-log']
    ),
    // Object.assign is important here because we need to assign
    // mocked properties directly to npmlog and then mock with that
    // object. This is necessary so tests can still directly set
    // `log.level = 'silent'` anywhere in the test and have that
    // that reflected in the npmlog singleton.
    // XXX: remove with npmlog
    npmlog: Object.assign(NPMLOG, merge(
      {
        log: (level, ...args) => {
          // timing does not exist on proclog, so if it got logged
          // with npmlog we need to push it to our logs
          if (level === 'timing') {
            logs.push([level, ...args])
          }
          npmLog(level, ...args)
        },
        write: (msg) => {
          // npmlog.write is what outputs to the terminal.
          // it writes in chunks so we push each chunk to an
          // array that we will log and zero out
          npmLogBuffer.push(msg)
        },
        emitLog: (m) => {
          // this calls the original emitLog method
          // which will filter based on loglevel
          npmEmitLog(m)
          // if anything was logged then we push to our display
          // array which we can assert against in tests
          if (npmLogBuffer.length) {
            // first two parts are 'npm' and a single space
            display.push(npmLogBuffer.slice(2))
          }
          npmLogBuffer.length = 0
        },
        newItem: () => {
          return {
            info: (...p) => {
              logs.push(['info', ...p])
            },
            warn: (...p) => {
              logs.push(['warn', ...p])
            },
            error: (...p) => {
              logs.push(['error', ...p])
            },
            silly: (...p) => {
              logs.push(['silly', ...p])
            },
            completeWork: () => {},
            finish: () => {},
          }
        },
      },
      otherMocks.npmlog
    )),
  }

  return { logs, logMocks, display }
}

module.exports = mockLogs
