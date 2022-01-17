const spawn = require('@npmcli/promise-spawn')
const promiseRetry = require('promise-retry')
const makeError = require('./make-error.js')
const whichGit = require('./which.js')
const makeOpts = require('./opts.js')
const procLog = require('./proc-log.js')

module.exports = (gitArgs, opts = {}) => {
  const gitPath = whichGit(opts)

  if (gitPath instanceof Error) { return Promise.reject(gitPath) }

  // undocumented option, mostly only here for tests
  const args = opts.allowReplace || gitArgs[0] === '--no-replace-objects'
    ? gitArgs
    : ['--no-replace-objects', ...gitArgs]

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
      log.silly('git', `Retrying git command: ${
        args.join(' ')} attempt # ${number}`)
    }

    return spawn(gitPath, args, makeOpts(opts))
      .catch(er => {
        const gitError = makeError(er)
        if (!gitError.shouldRetry(number)) {
          throw gitError
        }
        retry(gitError)
      })
  }, retry)
}
