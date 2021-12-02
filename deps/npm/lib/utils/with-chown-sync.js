const mkdirp = require('mkdirp-infer-owner')
const fs = require('graceful-fs')
const path = require('path')

module.exports = (file, method) => {
  const dir = path.dirname(file)
  mkdirp.sync(dir)
  const result = method(file)
  const st = fs.lstatSync(dir)
  fs.chownSync(dir, st.uid, st.gid)
  fs.chownSync(file, st.uid, st.gid)
  return result
}
