'use strict'

const defaultMaxRunning = 50

const limit = module.exports = function (func, maxRunning) {
  const state = {running: 0, queue: []}
  if (!maxRunning) maxRunning = defaultMaxRunning
  return function limited () {
    const args = Array.prototype.slice.call(arguments)
    if (state.running >= maxRunning) {
      state.queue.push({obj: this, args})
    } else {
      callFunc(this, args)
    }
  }
  function callNext () {
    if (state.queue.length === 0) return
    const next = state.queue.shift()
    callFunc(next.obj, next.args)
  }
  function callFunc (obj, args) {
    const cb = typeof args[args.length - 1] === 'function' && args.pop()
    try {
      ++state.running
      func.apply(obj, args.concat(function () {
        --state.running
        process.nextTick(callNext)
        if (cb) process.nextTick(() => cb.apply(obj, arguments))
      }))
    } catch (err) {
      --state.running
      if (cb) process.nextTick(() => cb.call(obj, err))
      process.nextTick(callNext)
    }
  }
}

module.exports.method = function (classOrObj, method, maxRunning) {
  if (typeof classOrObj === 'function') {
    const func = classOrObj.prototype[method]
    classOrObj.prototype[method] = limit(func, maxRunning)
  } else {
    const func = classOrObj[method]
    classOrObj[method] = limit(func, maxRunning)
  }
}

module.exports.promise = function (func, maxRunning) {
  const state = {running: 0, queue: []}
  if (!maxRunning) maxRunning = defaultMaxRunning
  return function limited () {
    const args = Array.prototype.slice.call(arguments)
    if (state.running >= maxRunning) {
      return new Promise(resolve => {
        state.queue.push({resolve, obj: this, args})
      })
    } else {
      return callFunc(this, args)
    }
  }
  function callNext () {
    if (state.queue.length === 0) return
    const next = state.queue.shift()
    next.resolve(callFunc(next.obj, next.args))
  }
  function callFunc (obj, args) {
    return callFinally(() => {
      ++state.running
      return func.apply(obj, args)
    }, () => {
      --state.running
      process.nextTick(callNext)
    })
  }
  function callFinally (action, fin) {
    try {
      return Promise.resolve(action()).then(value => {
        fin()
        return value
      }, err => {
        fin()
        return Promise.reject(err)
      })
    } catch (err) {
      fin()
      return Promise.reject(err)
    }
  }
}

module.exports.promise.method = function (classOrObj, method, maxRunning) {
  if (typeof classOrObj === 'function') {
    const func = classOrObj.prototype[method]
    classOrObj.prototype[method] = limit.promise(func, maxRunning)
  } else {
    const func = classOrObj[method]
    classOrObj[method] = limit.promise(func, maxRunning)
  }
}
