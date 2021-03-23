// ping the npm registry
// used by the ping and doctor commands
const fetch = require('npm-registry-fetch')
module.exports = async (flatOptions) => {
  const res = await fetch('/-/ping?write=true', flatOptions)
  return res.json().catch(() => ({}))
}
