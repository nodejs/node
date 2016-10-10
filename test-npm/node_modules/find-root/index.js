var path = require('path')
var fs = require('fs')

function findRoot(start) {
  start = start || module.parent.filename
  if (typeof start === 'string') {
    if (start[start.length-1] !== path.sep) {
      start+=path.sep
    }
    start = start.split(path.sep)
  }
  if(!start.length) {
    throw new Error('package.json not found in path')
  }
  start.pop()
  var dir = start.join(path.sep)
  try {
    fs.statSync(path.join(dir, 'package.json'));
    return dir;
  } catch (e) {}
  return findRoot(start)
}

module.exports = findRoot
