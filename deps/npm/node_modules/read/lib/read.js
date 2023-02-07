const readline = require('readline')
const Mute = require('mute-stream')

module.exports = async function read ({
  default: def = '',
  input = process.stdin,
  output = process.stdout,
  prompt = '',
  silent,
  timeout,
  edit,
  terminal,
  replace,
}) {
  if (typeof def !== 'undefined' && typeof def !== 'string' && typeof def !== 'number') {
    throw new Error('default value must be string or number')
  }

  let editDef = false
  prompt = prompt.trim() + ' '
  terminal = !!(terminal || output.isTTY)

  if (def) {
    if (silent) {
      prompt += '(<default hidden>) '
    } else if (edit) {
      editDef = true
    } else {
      prompt += '(' + def + ') '
    }
  }

  const m = new Mute({ replace, prompt })
  m.pipe(output, { end: false })
  output = m

  return new Promise((resolve, reject) => {
    const rl = readline.createInterface({ input, output, terminal })
    const timer = timeout && setTimeout(() => onError(new Error('timed out')), timeout)

    output.unmute()
    rl.setPrompt(prompt)
    rl.prompt()

    if (silent) {
      output.mute()
    } else if (editDef) {
      rl.line = def
      rl.cursor = def.length
      rl._refreshLine()
    }

    const done = () => {
      rl.close()
      clearTimeout(timer)
      output.mute()
      output.end()
    }

    const onError = (er) => {
      done()
      reject(er)
    }

    rl.on('error', onError)
    rl.on('line', (line) => {
      if (silent && terminal) {
        output.unmute()
        output.write('\r\n')
      }
      done()
      // truncate the \n at the end.
      const res = line.replace(/\r?\n$/, '') || def || ''
      return resolve(res)
    })

    rl.on('SIGINT', () => {
      rl.close()
      onError(new Error('canceled'))
    })
  })
}
