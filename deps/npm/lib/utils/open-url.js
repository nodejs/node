const promiseSpawn = require('@npmcli/promise-spawn')

const { URL } = require('url')

// attempt to open URL in web-browser, print address otherwise:
const open = async (npm, url, errMsg, isFile) => {
  url = encodeURI(url)
  const browser = npm.config.get('browser')

  function printAlternateMsg () {
    const json = npm.config.get('json')
    const alternateMsg = json
      ? JSON.stringify({
        title: errMsg,
        url,
      }, null, 2)
      : `${errMsg}:\n  ${url}\n`

    npm.output(alternateMsg)
  }

  if (browser === false) {
    printAlternateMsg()
    return
  }

  // We pass this in as true from the help command so we know we don't have to
  // check the protocol
  if (!isFile) {
    try {
      if (!/^https?:$/.test(new URL(url).protocol)) {
        throw new Error()
      }
    } catch {
      throw new Error('Invalid URL: ' + url)
    }
  }

  const command = browser === true ? null : browser
  await promiseSpawn.open(url, { command })
    .catch((err) => {
      if (err.code !== 'ENOENT') {
        throw err
      }

      printAlternateMsg()
    })
}

module.exports = open
