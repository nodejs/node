const t = require('tap')
let runArgs
const npm = {
  commands: {
    'run-script': (args, cb) => {
      runArgs = args
      cb()
    },
  },
}
const Stop = require('../../lib/stop.js')
const stop = new Stop(npm)
stop.exec(['foo'], () => {
  t.match(runArgs, ['stop', 'foo'])
  t.end()
})
