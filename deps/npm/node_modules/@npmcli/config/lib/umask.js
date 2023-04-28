class Umask {}
const parse = val => {
  if (typeof val === 'string') {
    if (/^0o?[0-7]+$/.test(val)) {
      return parseInt(val.replace(/^0o?/, ''), 8)
    } else if (/^[1-9][0-9]*$/.test(val)) {
      return parseInt(val, 10)
    } else {
      throw new Error(`invalid umask value: ${val}`)
    }
  }
  if (typeof val !== 'number') {
    throw new Error(`invalid umask value: ${val}`)
  }
  val = Math.floor(val)
  if (val < 0 || val > 511) {
    throw new Error(`invalid umask value: ${val}`)
  }
  return val
}

const validate = (data, k, val) => {
  try {
    data[k] = parse(val)
    return true
  } catch (er) {
    return false
  }
}

module.exports = { Umask, parse, validate }
