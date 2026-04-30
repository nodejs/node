const { RetryOperation } = require('./retry')

const createTimeout = (attempt, opts) => Math.min(Math.round((1 + (opts.randomize ? Math.random() : 0)) * Math.max(opts.minTimeout, 1) * Math.pow(opts.factor, attempt)), opts.maxTimeout)
const isRetryError = err => err?.code === 'EPROMISERETRY' && Object.hasOwn(err, 'retried')

const promiseRetry = async (fn, options = {}) => {
  let timeouts = []
  if (options instanceof Array) {
    timeouts = [...options]
  } else {
    if (options.retries === Infinity) {
      options.forever = true
      delete options.retries
    }
    const opts = {
      retries: 10,
      factor: 2,
      minTimeout: 1 * 1000,
      maxTimeout: Infinity,
      randomize: false,
      ...options
    }
    if (opts.minTimeout > opts.maxTimeout) {
      throw new Error('minTimeout is greater than maxTimeout')
    }
    if (opts.retries) {
      for (let i = 0; i < opts.retries; i++) {
        timeouts.push(createTimeout(i, opts))
      }
      // sort the array numerically ascending (since the timeouts may be out of order at factor < 1)
      timeouts.sort((a, b) => a - b)
    } else if (options.forever) {
      timeouts.push(createTimeout(0, opts))
    }
  }

  const operation = new RetryOperation(timeouts, {
    forever: options.forever,
    unref: options.unref,
    maxRetryTime: options.maxRetryTime
  })

  return new Promise(function (resolve, reject) {
    operation.attempt(async number => {
      try {
        const result = await fn(err => {
          throw Object.assign(new Error('Retrying'), { code: 'EPROMISERETRY', retried: err })
        }, number, operation)
        return resolve(result)
      } catch (err) {
        if (!isRetryError(err)) {
          return reject(err)
        }
        if (!operation.retry(err.retried || new Error())) {
          return reject(err.retried)
        }
      }
    })
  })
}

module.exports = { promiseRetry }
