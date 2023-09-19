const tar = require('tar')
const { minimatch } = require('minimatch')

const normalizeMatch = str => str
  .replace(/\\+/g, '/')
  .replace(/^\.\/|^\./, '')

// files and refs are mutating params
// filterFiles, item, prefix and opts are read-only options
const untar = ({ files, refs }, { filterFiles, item, prefix }) => {
  tar.list({
    filter: (path, entry) => {
      const fileMatch = () =>
        (!filterFiles.length ||
          filterFiles.some(f => {
            const pattern = normalizeMatch(f)
            return minimatch(
              normalizeMatch(path),
              `{package/,}${pattern}`,
              { matchBase: pattern.startsWith('*') }
            )
          }))

      // expands usage of simple path filters, e.g: lib or src/
      const folderMatch = () =>
        filterFiles.some(f =>
          normalizeMatch(path).startsWith(normalizeMatch(f)) ||
          normalizeMatch(path).startsWith(`package/${normalizeMatch(f)}`))

      if (
        entry.type === 'File' &&
        (fileMatch() || folderMatch())
      ) {
        const key = path.replace(/^[^/]+\/?/, '')
        files.add(key)

        // should skip reading file when using --name-only option
        let content
        try {
          entry.setEncoding('utf8')
          content = entry.concat()
        } catch (e) {
          /* istanbul ignore next */
          throw Object.assign(
            new Error('failed to read files'),
            { code: 'EDIFFUNTAR' }
          )
        }

        refs.set(`${prefix}${key}`, {
          content,
          mode: `100${entry.mode.toString(8)}`,
        })
        return true
      }
    },
  })
    .on('error', /* istanbul ignore next */ e => {
      throw e
    })
    .end(item)
}

const readTarballs = async (tarballs, opts = {}) => {
  const files = new Set()
  const refs = new Map()
  const arr = [].concat(tarballs)

  const filterFiles = opts.diffFiles || []

  for (const i of arr) {
    untar({
      files,
      refs,
    }, {
      item: i.item,
      prefix: i.prefix,
      filterFiles,
    })
  }

  // await to read all content from included files
  const allRefs = [...refs.values()]
  const contents = await Promise.all(allRefs.map(async ref => ref.content))

  contents.forEach((content, index) => {
    allRefs[index].content = content
  })

  return {
    files,
    refs,
  }
}

module.exports = readTarballs
