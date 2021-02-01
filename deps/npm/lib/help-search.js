const fs = require('fs')
const path = require('path')
const npm = require('./npm.js')
const color = require('ansicolors')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const { promisify } = require('util')
const glob = promisify(require('glob'))
const readFile = promisify(fs.readFile)
const didYouMean = require('./utils/did-you-mean.js')
const { cmdList } = require('./utils/cmd-list.js')

const usage = usageUtil('help-search', 'npm help-search <text>')
const completion = require('./utils/completion/none.js')

const npmUsage = require('./utils/npm-usage.js')

const cmd = (args, cb) => helpSearch(args).then(() => cb()).catch(cb)

const helpSearch = async args => {
  if (!args.length)
    throw usage

  const docPath = path.resolve(__dirname, '..', 'docs/content')

  const files = await glob(`${docPath}/*/*.md`)
  const data = await readFiles(files)
  const results = await searchFiles(args, data, files)
  // if only one result, then just show that help section.
  if (results.length === 1) {
    return npm.commands.help([path.basename(results[0].file, '.md')], er => {
      if (er)
        throw er
    })
  }

  const formatted = formatResults(args, results)
  if (!formatted.trim())
    npmUsage(false)
  else {
    output(formatted)
    output(didYouMean(args[0], cmdList))
  }
}

const readFiles = async files => {
  const res = {}
  await Promise.all(files.map(async file => {
    res[file] = (await readFile(file, 'utf8'))
      .replace(/^---\n(.*\n)*?---\n/, '').trim()
  }))
  return res
}

const searchFiles = async (args, data, files) => {
  const results = []
  for (const [file, content] of Object.entries(data)) {
    const lowerCase = content.toLowerCase()
    // skip if no matches at all
    if (!args.some(a => lowerCase.includes(a.toLowerCase())))
      continue

    const lines = content.split(/\n+/)

    // if a line has a search term, then skip it and the next line.
    // if the next line has a search term, then skip all 3
    // otherwise, set the line to null.  then remove the nulls.
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i]
      const nextLine = lines[i + 1]
      let match = false
      if (nextLine) {
        match = args.some(a => nextLine.toLowerCase().includes(a.toLowerCase()))
        if (match) {
          // skip over the next line, and the line after it.
          i += 2
          continue
        }
      }

      match = args.some(a => line.toLowerCase().includes(a.toLowerCase()))

      if (match) {
        // skip over the next line
        i++
        continue
      }

      lines[i] = null
    }

    // now squish any string of nulls into a single null
    const pruned = lines.reduce((l, r) => {
      if (!(r === null && l[l.length - 1] === null))
        l.push(r)

      return l
    }, [])

    if (pruned[pruned.length - 1] === null)
      pruned.pop()

    if (pruned[0] === null)
      pruned.shift()

    // now count how many args were found
    const found = {}
    let totalHits = 0
    for (const line of pruned) {
      for (const arg of args) {
        const hit = (line || '').toLowerCase()
          .split(arg.toLowerCase()).length - 1

        if (hit > 0) {
          found[arg] = (found[arg] || 0) + hit
          totalHits += hit
        }
      }
    }

    const cmd = 'npm help ' +
      path.basename(file, '.md').replace(/^npm-/, '')
    results.push({
      file,
      cmd,
      lines: pruned,
      found: Object.keys(found),
      hits: found,
      totalHits,
    })
  }

  // sort results by number of results found, then by number of hits
  // then by number of matching lines

  // coverage is ignored here because the contents of results are
  // nondeterministic due to either glob or readFiles or Object.entries
  return results.sort(/* istanbul ignore next */ (a, b) =>
    a.found.length > b.found.length ? -1
    : a.found.length < b.found.length ? 1
    : a.totalHits > b.totalHits ? -1
    : a.totalHits < b.totalHits ? 1
    : a.lines.length > b.lines.length ? -1
    : a.lines.length < b.lines.length ? 1
    : 0).slice(0, 10)
}

const formatResults = (args, results) => {
  const cols = Math.min(process.stdout.columns || Infinity, 80) + 1

  const out = results.map(res => {
    const out = [res.cmd]
    const r = Object.keys(res.hits)
      .map(k => `${k}:${res.hits[k]}`)
      .sort((a, b) => a > b ? 1 : -1)
      .join(' ')

    out.push(' '.repeat((Math.max(1, cols - out.join(' ').length - r.length - 1))))
    out.push(r)

    if (!npm.flatOptions.long)
      return out.join('')

    out.unshift('\n\n')
    out.push('\n')
    out.push('-'.repeat(cols - 1) + '\n')
    res.lines.forEach((line, i) => {
      if (line === null || i > 3)
        return

      if (!npm.color) {
        out.push(line + '\n')
        return
      }
      const hilitLine = []
      for (const arg of args) {
        const finder = line.toLowerCase().split(arg.toLowerCase())
        let p = 0
        for (const f of finder) {
          hilitLine.push(line.substr(p, f.length))
          const word = line.substr(p + f.length, arg.length)
          const hilit = color.bgBlack(color.red(word))
          hilitLine.push(hilit)
          p += f.length + arg.length
        }
      }
      out.push(hilitLine.join('') + '\n')
    })

    return out.join('')
  }).join('\n')

  const finalOut = results.length && !npm.flatOptions.long
    ? 'Top hits for ' + (args.map(JSON.stringify).join(' ')) + '\n' +
      '—'.repeat(cols - 1) + '\n' +
      out + '\n' +
      '—'.repeat(cols - 1) + '\n' +
      '(run with -l or --long to see more context)'
    : out

  return finalOut.trim()
}

module.exports = Object.assign(cmd, { usage, completion })
