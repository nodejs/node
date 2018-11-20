"use strict"

var defaultMaxRunning = 50

var limit = module.exports = function (func, maxRunning) {
  var running = 0
  var queue = []
  if (!maxRunning) maxRunning = defaultMaxRunning
  return function limited () {
    var self = this
    var args = Array.prototype.slice.call(arguments)
    if (running >= maxRunning) {
      queue.push({self: this, args: args})
      return
    }
    var cb = typeof args[args.length-1] === 'function' && args.pop()
    ++ running
    args.push(function () {
      var cbargs = arguments
      -- running
      cb && process.nextTick(function () {
        cb.apply(self, cbargs)
      })
      if (queue.length) {
        var next = queue.shift()
        limited.apply(next.self, next.args)
      }
    })
    func.apply(self, args)
  }
}

module.exports.method = function (classOrObj, method, maxRunning) {
  if (typeof classOrObj === 'function') {
    var func = classOrObj.prototype[method]
    classOrObj.prototype[method] = limit(func, maxRunning)
  } else {
    var func = classOrObj[method]
    classOrObj[method] = limit(func, maxRunning)
  }
}

module.exports.promise = function (func, maxRunning) {
  var running = 0
  var queue = []
  if (!maxRunning) maxRunning = defaultMaxRunning
  return function () {
    var self = this
    var args = Array.prototype.slice.call(arguments)
    return new Promise(function (resolve) {
      if (running >= maxRunning) {
        queue.push({self: self, args: args, resolve: resolve})
        return
      } else {
        runNext(self, args, resolve)
      }
      function runNext (self, args, resolve) {
        ++ running
        resolve(
          func.apply(self, args)
          .then(finish, function (err) {
            finish(err)
            throw err
           }))
      }

      function finish () {
        -- running
        if (queue.length) {
          var next = queue.shift()
          process.nextTick(runNext, next.self, next.args, next.resolve)
        }
      }
    })
  }
}

module.exports.promise.method = function (classOrObj, method, maxRunning) {
  if (typeof classOrObj === 'function') {
    var func = classOrObj.prototype[method]
    classOrObj.prototype[method] = limit.promise(func, maxRunning)
  } else {
    var func = classOrObj[method]
    classOrObj[method] = limit.promise(func, maxRunning)
  }
}
