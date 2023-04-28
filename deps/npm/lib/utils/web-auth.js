const EventEmitter = require('events')
const { webAuthCheckLogin } = require('npm-profile')

async function webAuth (opener, initialUrl, doneUrl, opts) {
  const doneEmitter = new EventEmitter()

  const openPromise = opener(initialUrl, doneEmitter)
  const webAuthCheckPromise = webAuthCheckLogin(doneUrl, { ...opts, cache: false })
    .then(authResult => {
      // cancel open prompt if it's present
      doneEmitter.emit('abort')

      return authResult.token
    })

  await openPromise
  return await webAuthCheckPromise
}

module.exports = webAuth
