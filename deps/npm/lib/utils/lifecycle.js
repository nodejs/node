exports = module.exports = runLifecycle

const lifecycleOpts = require('../config/lifecycle')
const lifecycle = require('npm-lifecycle')

function runLifecycle (pkg, stage, wd, moreOpts, cb) {
  if (typeof moreOpts === 'function') {
    cb = moreOpts
    moreOpts = null
  }

  const opts = lifecycleOpts(moreOpts)
  lifecycle(pkg, stage, wd, opts).then(cb, cb)
}
