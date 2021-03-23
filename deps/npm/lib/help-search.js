const fs = require('fs')
const path = require('path')
const color = require('ansicolors')
const { promisify } = require('util')
const glob = promisify(require('glob'))
const readFile = promisify(fs.readFile)
const BaseCommand = require('./base-command.js')

class HelpSearch extends BaseCommand {
  static get description () {
    return 'Search npm help documentation'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'help-search'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['<text>']
  }

  exec (args, cb) {
    this.helpSearch(args).then(() => cb()).catch(cb)
  }

  async helpSearch (args) {
    if (!args.length)
      return this.npm.output(this.usage)

    const docPath = path.resolve(__dirname, '..', 'docs/content')
    const files = await glob(`${docPath}/*/*.md`)
    const data = await this.readFiles(files)
    const results = await this.searchFiles(args, data, files)
    const formatted = this.formatResults(args, results)
    if (!formatted.trim())
      this.npm.output(`No matches in help for: ${args.join(' ')}\n`)
    else
      this.npm.output(formatted)
  }

  async readFiles (files) {
    const res = {}
    await Promise.all(files.map(async file => {
      res[file] = (await readFile(file, 'utf8'))
        .replace(/^---\n(.*\n)*?---\n/, '').trim()
    }))
    return res
  }

  async searchFiles (args, data, files) {
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
          match = args.some(a =>
            nextLine.toLowerCase().includes(a.toLowerCase()))
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

  formatResults (args, results) {
    const cols = Math.min(process.stdout.columns || Infinity, 80) + 1

    const out = results.map(res => {
      const out = [res.cmd]
      const r = Object.keys(res.hits)
        .map(k => `${k}:${res.hits[k]}`)
        .sort((a, b) => a > b ? 1 : -1)
        .join(' ')

      out.push(' '.repeat((Math.max(1, cols - out.join(' ').length - r.length - 1))))
      out.push(r)

      if (!this.npm.config.get('long'))
        return out.join('')

      out.unshift('\n\n')
      out.push('\n')
      out.push('-'.repeat(cols - 1) + '\n')
      res.lines.forEach((line, i) => {
        if (line === null || i > 3)
          return

        if (!this.npm.color) {
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

    const finalOut = results.length && !this.npm.config.get('long')
      ? 'Top hits for ' + (args.map(JSON.stringify).join(' ')) + '\n' +
      '—'.repeat(cols - 1) + '\n' +
      out + '\n' +
      '—'.repeat(cols - 1) + '\n' +
      '(run with -l or --long to see more context)'
      : out

    return finalOut.trim()
  }
}
module.exports = HelpSearch
