const defLimit = require('os').cpus().length
const callLimit = (queue, limit = defLimit) => new Promise((res, rej) => {
  let active = 0
  let current = 0
  const results = []

  let rejected = false
  const reject = er => {
    if (rejected)
      return
    rejected = true
    rej(er)
  }

  let resolved = false
  const resolve = () => {
    if (resolved || active > 0)
      return
    resolved = true
    res(results)
  }

  const run = () => {
    const c = current++
    if (c >= queue.length) {
      return resolve()
    }

    active ++
    results[c] = queue[c]().then(result => {
      active --
      results[c] = result
      run()
      return result
    }, reject)
  }

  for (let i = 0; i < limit; i++) {
    run()
  }
})

module.exports = callLimit
