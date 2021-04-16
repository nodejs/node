const spawn = require('@npmcli/promise-spawn')
const promiseRetry = require('promise-retry')
const shouldRetry = require('./should-retry.js')
const whichGit = require('./which.js')
const makeOpts = require('./opts.js')
const procLog = require('./proc-log.js')

module.exports = (gitArgs, opts = {}) => {
  const gitPath = whichGit(opts)

  if (gitPath instanceof Error) { return Promise.reject(gitPath) }

  const log = opts.log || procLog
  let retry = opts.retry
  if (retry === null || retry === undefined) {
    retry = {
      retries: opts.fetchRetries || 2,
      factor: opts.fetchRetryFactor || 10,
      maxTimeout: opts.fetchRetryMaxtimeout || 60000,
      minTimeout: opts.fetchRetryMintimeout || 1000
    }
  }
  return promiseRetry((retry, number) => {
    if (number !== 1) {
      log.silly('pacote', `Retrying git command: ${
        gitArgs.join(' ')} attempt # ${number}`)
    }

    return spawn(gitPath, gitArgs, makeOpts(opts))
      .catch(er => {
        if (!shouldRetry(er.stderr, number)) {
          throw er
        }
        retry(er)
      })
  }, retry)
}
