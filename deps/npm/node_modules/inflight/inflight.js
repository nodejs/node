module.exports = inflight

var reqs = Object.create(null)
var once = require('once')

function inflight (key, cb) {
  if (reqs[key]) {
    reqs[key].push(cb)
    return null
  } else {
    reqs[key] = [cb]
    return makeres(key)
  }
}

function makeres(key) {
  return once(res)
  function res(error,  data) {
    var cbs = reqs[key]
    delete reqs[key]
    cbs.forEach(function(cb) {
      cb(error, data)
    })
  }
}
