const allSettled =
  Promise.allSettled ? promises => Promise.allSettled(promises)
  : promises => {
    const reflections = []
    for (let i = 0; i < promises.length; i++) {
      reflections[i] = Promise.resolve(promises[i]).then(value => ({
        status: 'fulfilled',
        value,
      }), reason => ({
        status: 'rejected',
        reason,
      }))
    }
    return Promise.all(reflections)
  }

module.exports = promises => allSettled(promises).then(results => {
  let er = null
  const ret = new Array(results.length)
  results.forEach((result, i) => {
    if (result.status === 'rejected')
      throw result.reason
    else
      ret[i] = result.value
  })
  return ret
})
