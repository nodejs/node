const readline = require('readline')
const { output } = require('proc-log')
const open = require('./open-url.js')

function print (npm, title, url) {
  const json = npm.config.get('json')

  const message = json ? JSON.stringify({ title, url }) : `${title}:\n${url}`

  output.standard(message)
}

// Prompt to open URL in browser if possible
const promptOpen = async (npm, url, title, prompt, emitter) => {
  const browser = npm.config.get('browser')
  const isInteractive = process.stdin.isTTY === true && process.stdout.isTTY === true

  try {
    if (!/^https?:$/.test(new URL(url).protocol)) {
      throw new Error()
    }
  } catch (_) {
    throw new Error('Invalid URL: ' + url)
  }

  print(npm, title, url)

  if (browser === false || !isInteractive) {
    return
  }

  const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
  })

  const tryOpen = await new Promise(resolve => {
    rl.on('SIGINT', () => {
      rl.close()
      resolve('SIGINT')
    })

    rl.question(prompt, () => {
      resolve(true)
    })

    if (emitter && emitter.addListener) {
      emitter.addListener('abort', () => {
        rl.close()

        // clear the prompt line
        output.standard('')

        resolve(false)
      })
    }
  })

  if (tryOpen === 'SIGINT') {
    throw new Error('canceled')
  }

  if (!tryOpen) {
    return
  }

  await open(npm, url, 'Browser unavailable.  Please open the URL manually')
}

module.exports = promptOpen
