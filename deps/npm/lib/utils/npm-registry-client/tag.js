
module.exports = tag

var PUT = require("./request.js").PUT

function tag (project, version, tag, cb) {
  PUT(project+"/"+tag, JSON.stringify(version), cb)
}
