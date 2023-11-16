const { Minipass } = require('minipass')
const columnify = require('columnify')

// This module consumes package data in the following format:
//
// {
//   name: String,
//   description: String,
//   maintainers: [{ username: String, email: String }],
//   keywords: String | [String],
//   version: String,
//   date: Date // can be null,
// }
//
// The returned stream will format this package data
// into a byte stream of formatted, displayable output.

module.exports = async (opts, clean) => {
  return opts.json ? new JSONOutputStream() : new TextOutputStream(opts, clean)
}

class JSONOutputStream extends Minipass {
  #didFirst = false

  write (obj) {
    if (!this.#didFirst) {
      super.write('[\n')
      this.#didFirst = true
    } else {
      super.write('\n,\n')
    }

    return super.write(JSON.stringify(obj))
  }

  end () {
    super.write(this.#didFirst ? ']\n' : '\n[]\n')
    super.end()
  }
}

class TextOutputStream extends Minipass {
  #clean
  #opts
  #line = 0

  constructor (opts, clean) {
    super()
    this.#clean = clean
    this.#opts = opts
  }

  write (pkg) {
    return super.write(this.#prettify(pkg))
  }

  #prettify (data) {
    const pkg = {
      author: data.maintainers.map((m) => `=${this.#clean(m.username)}`).join(' '),
      date: 'prehistoric',
      description: this.#clean(data.description ?? ''),
      keywords: '',
      name: this.#clean(data.name),
      version: data.version,
    }
    if (Array.isArray(data.keywords)) {
      pkg.keywords = data.keywords.map((k) => this.#clean(k)).join(' ')
    } else if (typeof data.keywords === 'string') {
      pkg.keywords = this.#clean(data.keywords.replace(/[,\s]+/, ' '))
    }
    if (data.date) {
      pkg.date = data.date.toISOString().split('T')[0] // remove time
    }

    const columns = ['name', 'description', 'author', 'date', 'version', 'keywords']
    if (this.#opts.parseable) {
      return columns.map((col) => pkg[col] && ('' + pkg[col]).replace(/\t/g, ' ')).join('\t')
    }

    // stdout in tap is never a tty
    /* istanbul ignore next */
    const maxWidth = process.stdout.isTTY ? process.stdout.getWindowSize()[0] : Infinity
    let output = columnify(
      [pkg],
      {
        include: columns,
        showHeaders: ++this.#line <= 1,
        columnSplitter: ' | ',
        truncate: !this.#opts.long,
        config: {
          name: { minWidth: 25, maxWidth: 25, truncate: false, truncateMarker: '' },
          description: { minWidth: 20, maxWidth: 20 },
          author: { minWidth: 15, maxWidth: 15 },
          date: { maxWidth: 11 },
          version: { minWidth: 8, maxWidth: 8 },
          keywords: { maxWidth: Infinity },
        },
      }
    ).split('\n').map(line => line.slice(0, maxWidth)).join('\n')

    if (!this.#opts.color) {
      return output
    }

    const colors = ['31m', '33m', '32m', '36m', '34m', '35m']

    this.#opts.args.forEach((arg, i) => {
      const markStart = String.fromCharCode(i % colors.length + 1)
      const markEnd = String.fromCharCode(0)

      if (arg.charAt(0) === '/') {
        output = output.replace(
          new RegExp(arg.slice(1, -1), 'gi'),
          bit => `${markStart}${bit}${markEnd}`
        )
      } else {
        // just a normal string, do the split/map thing
        let p = 0

        output = output.toLowerCase().split(arg.toLowerCase()).map(piece => {
          piece = output.slice(p, p + piece.length)
          p += piece.length
          const mark = `${markStart}${output.slice(p, p + arg.length)}${markEnd}`
          p += arg.length
          return `${piece}${mark}`
        }).join('')
      }
    })

    for (let i = 1; i <= colors.length; i++) {
      output = output.split(String.fromCharCode(i)).join(`\u001B[${colors[i - 1]}`)
    }
    return output.split('\u0000').join('\u001B[0m').trim()
  }
}
