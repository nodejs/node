const signals = require('./signals.js')

// for testing, expose the process being used
module.exports = Object.assign(fn => setup(fn), { process })

// do all of this in a setup function so that we can call it
// multiple times for multiple reifies that might be going on.
// Otherwise, Arborist.reify() is a global action, which is a
// new constraint we'd be adding with this behavior.
const setup = fn => {
  const { process } = module.exports

  const sigListeners = { loaded: false }

  const unload = () => {
    if (!sigListeners.loaded)
      return
    for (const sig of signals) {
      try {
        process.removeListener(sig, sigListeners[sig])
      } catch (er) {}
    }
    process.removeListener('beforeExit', onBeforeExit)
    sigListeners.loaded = false
  }

  const onBeforeExit = () => {
    // this trick ensures that we exit with the same signal we caught
    // Ie, if you press ^C and npm gets a SIGINT, we'll do the rollback
    // and then exit with a SIGINT signal once we've removed the handler.
    // The timeout is there because signals are asynchronous, so we need
    // the process to NOT exit on its own, which means we have to have
    // something keeping the event loop looping.  Hence this hack.
    unload()
    process.kill(process.pid, signalReceived)
    setTimeout(() => {}, 500)
  }

  let signalReceived = null
  const listener = (sig, fn) => () => {
    signalReceived = sig

    // if we exit normally, but caught a signal which would have been fatal,
    // then re-send it once we're done with whatever cleanup we have to do.
    unload()
    if (process.listeners(sig).length < 1)
      process.once('beforeExit', onBeforeExit)

    fn({ signal: sig })
  }

  // do the actual loading here
  for (const sig of signals) {
    sigListeners[sig] = listener(sig, fn)
    const max = process.getMaxListeners()
    try {
      // if we call this a bunch of times, avoid triggering the warning
      const { length } = process.listeners(sig)
      if (length >= max)
        process.setMaxListeners(length + 1)
      process.on(sig, sigListeners[sig])
    } catch (er) {}
  }
  sigListeners.loaded = true

  return unload
}
