const isObj = val => !!val && !Array.isArray(val) && typeof val === 'object'

const compare = (ak, bk, prefKeys) =>
  prefKeys.includes(ak) && !prefKeys.includes(bk) ? -1
  : prefKeys.includes(bk) && !prefKeys.includes(ak) ? 1
  : prefKeys.includes(ak) && prefKeys.includes(bk)
    ? prefKeys.indexOf(ak) - prefKeys.indexOf(bk)
  : ak.localeCompare(bk)

const sort = (replacer, seen) => (key, val) => {
  const prefKeys = Array.isArray(replacer) ? replacer : []

  if (typeof replacer === 'function')
    val = replacer(key, val)

  if (!isObj(val))
    return val

  if (seen.has(val))
    return seen.get(val)

  const ret = Object.entries(val).sort(
    ([ak, av], [bk, bv]) =>
      isObj(av) === isObj(bv) ? compare(ak, bk, prefKeys)
      : isObj(av) ? 1
      : -1
  ).reduce((set, [k, v]) => {
    set[k] = v
    return set
  }, {})

  seen.set(val, ret)
  return ret
}

module.exports = (obj, replacer, space = 2) =>
  JSON.stringify(obj, sort(replacer, new Map()), space)
  + (space ? '\n' : '')
