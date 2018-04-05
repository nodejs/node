var pump = require('pump')
var inherits = require('inherits')
var Duplexify = require('duplexify')

var toArray = function(args) {
  if (!args.length) return []
  return Array.isArray(args[0]) ? args[0] : Array.prototype.slice.call(args)
}

var define = function(opts) {
  var Pumpify = function() {
    var streams = toArray(arguments)
    if (!(this instanceof Pumpify)) return new Pumpify(streams)
    Duplexify.call(this, null, null, opts)
    if (streams.length) this.setPipeline(streams)
  }

  inherits(Pumpify, Duplexify)

  Pumpify.prototype.setPipeline = function() {
    var streams = toArray(arguments)
    var self = this
    var ended = false
    var w = streams[0]
    var r = streams[streams.length-1]

    r = r.readable ? r : null
    w = w.writable ? w : null

    var onclose = function() {
      streams[0].emit('error', new Error('stream was destroyed'))
    }

    this.on('close', onclose)
    this.on('prefinish', function() {
      if (!ended) self.cork()
    })

    pump(streams, function(err) {
      self.removeListener('close', onclose)
      if (err) return self.destroy(err)
      ended = true
      self.uncork()
    })

    if (this.destroyed) return onclose()
    this.setWritable(w)
    this.setReadable(r)
  }

  return Pumpify
}

module.exports = define({destroy:false})
module.exports.obj = define({destroy:false, objectMode:true, highWaterMark:16})
