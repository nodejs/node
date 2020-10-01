// ping the npm registry
// used by the ping and doctor commands
const fetch = require('npm-registry-fetch')
module.exports = async (opts) => {
  const res = await fetch('/-/ping?write=true', opts)
  return res.json().catch(() => ({}))
}
