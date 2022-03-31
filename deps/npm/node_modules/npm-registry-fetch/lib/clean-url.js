const { URL } = require('url')

const replace = '***'
const tokenRegex = /\bnpm_[a-zA-Z0-9]{36}\b/g
const guidRegex = /\b[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\b/g

const cleanUrl = (str) => {
  if (typeof str !== 'string' || !str) {
    return str
  }

  try {
    const url = new URL(str)
    if (url.password) {
      str = str.replace(url.password, replace)
    }
  } catch {}

  return str
    .replace(tokenRegex, `npm_${replace}`)
    .replace(guidRegex, `npm_${replace}`)
}

module.exports = cleanUrl
