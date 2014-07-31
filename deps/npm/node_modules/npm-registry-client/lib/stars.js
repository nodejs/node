var url = require("url")

module.exports = stars

function stars (base, name, cb) {
  name = encodeURIComponent(name)
  var path = "/-/_view/starredByUser?key=\""+name+"\""
  this.request("GET", url.resolve(base, path), null, cb)
}
