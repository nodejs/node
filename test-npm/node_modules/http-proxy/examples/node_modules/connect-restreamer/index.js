
module.exports = function (options) {
  options = options || {}
  options.property = options.property || 'body'
  options.stringify = options.stringify || JSON.stringify

  return function (req, res, next) {
    req.removeAllListeners('data')
    req.removeAllListeners('end')
    next()
    process.nextTick(function () {
      if(req[options.property]) {
        if('function' === typeof options.modify)
          req[options.property] = options.modify(req[options.property])
        req.emit('data', options.stringify(req.body))
      }
      req.emit('end')
    })
  }
}