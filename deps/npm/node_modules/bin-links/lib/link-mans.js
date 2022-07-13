const { dirname, relative, join, resolve, basename } = require('path')
const linkGently = require('./link-gently.js')
const manTarget = require('./man-target.js')

const linkMans = ({ path, pkg, top, force }) => {
  const target = manTarget({ path, top })
  if (!target || !pkg.man || !Array.isArray(pkg.man) || !pkg.man.length) {
    return Promise.resolve([])
  }

  // break any links to c:\\blah or /foo/blah or ../blah
  // and filter out duplicates
  const set = [...new Set(pkg.man.map(man =>
    man ? join('/', man).replace(/\\|:/g, '/').slice(1) : null)
    .filter(man => typeof man === 'string'))]

  return Promise.all(set.map(man => {
    const parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
    if (!parseMan) {
      return Promise.reject(Object.assign(new Error('invalid man entry name\n' +
        'Man files must end with a number, ' +
        'and optionally a .gz suffix if they are compressed.'
      ), {
        code: 'EBADMAN',
        path,
        pkgid: pkg._id,
        man,
      }))
    }

    const stem = parseMan[1]
    const sxn = parseMan[2]
    const base = basename(stem)
    const absFrom = resolve(path, man)
    /* istanbul ignore if - that unpossible */
    if (absFrom.indexOf(path) !== 0) {
      return Promise.reject(Object.assign(new Error('invalid man entry'), {
        code: 'EBADMAN',
        path,
        pkgid: pkg._id,
        man,
      }))
    }

    const to = resolve(target, 'man' + sxn, base)
    const from = relative(dirname(to), absFrom)

    return linkGently({ from, to, path, absFrom, force })
  }))
}

module.exports = linkMans
