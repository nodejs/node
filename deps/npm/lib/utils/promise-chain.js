
module.exports = promiseChain

// usage:
//
// promiseChain(cb) <-- this is the callback for eventual success or error
//  ( fn, [arg, arg, arg], function (a,b,c) { success(a,b,c) })
//  ( fn2, [args] )
//  () <-- this kicks it off.
//
// promiseChain.call(someObj, cb) <-- bind this-context for all functions

function promiseChain (cb) {
  var steps = []
    , vals = []
    , context = this
  function go () {
    var step = steps.shift()
    if (!step) return cb()
    try { step[0].apply(context, step[1]) }
    catch (ex) { cb(ex) }
  }
  return function pc (fn, args, success) {
    if (arguments.length === 0) return go()
    // add the step
    steps.push
      ( [ fn
        , (args || []).concat([ function (er) {
            if (er) return cb(er)
            var a = Array.prototype.slice.call(arguments, 1)
            try { success && success.apply(context, a) }
            catch (ex) { return cb(ex) }
            go()
          }])
        ]
      )
    return pc
  }
}
