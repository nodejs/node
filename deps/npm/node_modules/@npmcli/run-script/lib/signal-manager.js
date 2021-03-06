const runningProcs = new Set()
let handlersInstalled = false

const forwardedSignals = [
  'SIGINT',
  'SIGTERM'
]

const handleSignal = signal => {
  for (const proc of runningProcs) {
    proc.kill(signal)
  }
}

const setupListeners = () => {
  for (const signal of forwardedSignals) {
    process.on(signal, handleSignal)
  }
  handlersInstalled = true
}

const cleanupListeners = () => {
  if (runningProcs.size === 0) {
    for (const signal of forwardedSignals) {
      process.removeListener(signal, handleSignal)
    }
    handlersInstalled = false
  }
}

const add = proc => {
  runningProcs.add(proc)
  if (!handlersInstalled)
    setupListeners()

  proc.once('exit', () => {
    runningProcs.delete(proc)
    cleanupListeners()
  })
}

module.exports = {
  add,
  handleSignal,
  forwardedSignals
}
