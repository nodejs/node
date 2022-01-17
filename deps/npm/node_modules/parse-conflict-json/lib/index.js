const parseJSON = require('json-parse-even-better-errors')
const { diff } = require('just-diff')
const { diffApply } = require('just-diff-apply')

const globalObjectProperties = Object.getOwnPropertyNames(Object.prototype)

const stripBOM = content => {
  content = content.toString()
  // Remove byte order marker. This catches EF BB BF (the UTF-8 BOM)
  // because the buffer-to-string conversion in `fs.readFileSync()`
  // translates it to FEFF, the UTF-16 BOM.
  if (content.charCodeAt(0) === 0xFEFF) {
    content = content.slice(1)
  }
  return content
}

const PARENT_RE = /\|{7,}/g
const OURS_RE = /<{7,}/g
const THEIRS_RE = /={7,}/g
const END_RE = />{7,}/g

const isDiff = str =>
  str.match(OURS_RE) && str.match(THEIRS_RE) && str.match(END_RE)

const parseConflictJSON = (str, reviver, prefer) => {
  prefer = prefer || 'ours'
  if (prefer !== 'theirs' && prefer !== 'ours') {
    throw new TypeError('prefer param must be "ours" or "theirs" if set')
  }

  str = stripBOM(str)

  if (!isDiff(str)) {
    return parseJSON(str)
  }

  const pieces = str.split(/[\n\r]+/g).reduce((acc, line) => {
    if (line.match(PARENT_RE)) {
      acc.state = 'parent'
    } else if (line.match(OURS_RE)) {
      acc.state = 'ours'
    } else if (line.match(THEIRS_RE)) {
      acc.state = 'theirs'
    } else if (line.match(END_RE)) {
      acc.state = 'top'
    } else {
      if (acc.state === 'top' || acc.state === 'ours') {
        acc.ours += line
      }
      if (acc.state === 'top' || acc.state === 'theirs') {
        acc.theirs += line
      }
      if (acc.state === 'top' || acc.state === 'parent') {
        acc.parent += line
      }
    }
    return acc
  }, {
    state: 'top',
    ours: '',
    theirs: '',
    parent: '',
  })

  // this will throw if either piece is not valid JSON, that's intended
  const parent = parseJSON(pieces.parent, reviver)
  const ours = parseJSON(pieces.ours, reviver)
  const theirs = parseJSON(pieces.theirs, reviver)

  return prefer === 'ours'
    ? resolve(parent, ours, theirs)
    : resolve(parent, theirs, ours)
}

const isObj = obj => obj && typeof obj === 'object'

const copyPath = (to, from, path, i) => {
  const p = path[i]
  if (isObj(to[p]) && isObj(from[p]) &&
      Array.isArray(to[p]) === Array.isArray(from[p])) {
    return copyPath(to[p], from[p], path, i + 1)
  }
  to[p] = from[p]
}

// get the diff from parent->ours and applying our changes on top of theirs.
// If they turned an object into a non-object, then put it back.
const resolve = (parent, ours, theirs) => {
  const dours = diff(parent, ours)
  for (let i = 0; i < dours.length; i++) {
    if (globalObjectProperties.find(prop => dours[i].path.includes(prop))) {
      continue
    }
    try {
      diffApply(theirs, [dours[i]])
    } catch (e) {
      copyPath(theirs, ours, dours[i].path, 0)
    }
  }
  return theirs
}

module.exports = Object.assign(parseConflictJSON, { isDiff })
