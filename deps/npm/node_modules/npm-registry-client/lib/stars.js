module.exports = stars

function stars (name, cb) {
  name = encodeURIComponent(name)
  var path = "/-/_view/starredByUser?key=\""+name+"\""
  this.request("GET", path, cb)
}
