
/**
 * An `asyncEmit()` function that accepts an EventEmitter, an Array of args, and
 * a callback function. If the emitter listener function has an arity
 * > args.length then there is an assumed callback function on the emitter, which
 * means that it is doing some async work. We have to wait for the callbacks for
 * any async listener functions.
 *
 * It works like this:
 *
 *   var emitter = new EventEmitter
 *
 *   // this is an async listener
 *   emitter.on('something', function (val, done) {
 *     // val may be any number of input arguments
 *     setTimeout(function () {
 *       done()
 *     }, 1000)
 *   })
 *
 *   // this is a sync listener, no callback function
 *   emitter.on('something', function (val) {
 *
 *   })
 *
 *   asyncEmit(emitter, 'something', [ 5 ], function (err) {
 *     if (err) throw err
 *     console.log('DONE!')
 *   })
 */

module.exports = asyncEmit
function asyncEmit (emitter, eventName, args, callback) {

  if (typeof args == 'function') {
    callback = args
    args = []
  }

  var async = emitter.listeners(eventName).filter(function (func) {
    return func.length > args.length
  }).length

  var argv = [ eventName ].concat(args)

  // callback function
  argv.push(function (err) {
    if (err && !callback.called) {
      callback.called = true
      callback(err)
    }
    --async || callback()
  })

  // no async listeners
  if (async === 0) {
    process.nextTick(callback)
  }

  return emitter.emit.apply(emitter, argv)
}
