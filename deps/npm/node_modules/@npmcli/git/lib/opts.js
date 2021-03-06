const gitEnv = require('./env.js')
module.exports = (opts = {}) => ({
  stdioString: true,
  ...opts,
  env: opts.env || gitEnv(),
})
