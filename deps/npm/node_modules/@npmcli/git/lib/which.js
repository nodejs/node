const which = require('which')

let gitPath
try {
  gitPath = which.sync('git')
} catch {
  // ignore errors
}

module.exports = (opts = {}) => {
  if (opts.git) {
    return opts.git
  }
  if (!gitPath || opts.git === false) {
    return Object.assign(new Error('No git binary found in $PATH'), { code: 'ENOGIT' })
  }
  return gitPath
}
