const retry = require('retry')

const isRetryError = (err) => err?.code === 'EPROMISERETRY' && Object.hasOwn(err, 'retried')

async function promiseRetry (fn, options = {}) {
  const operation = retry.operation(options)

  return new Promise(function (resolve, reject) {
    operation.attempt(async number => {
      try {
        const result = await fn(err => {
          throw Object.assign(new Error('Retrying'), { code: 'EPROMISERETRY', retried: err })
        }, number, operation)
        return resolve(result)
      } catch (err) {
        if (isRetryError(err)) {
          if (operation.retry(err.retried || new Error())) {
            return
          }
          return reject(err.retried)
        }
        return reject(err)
      }
    })
  })
}

module.exports = { promiseRetry }
