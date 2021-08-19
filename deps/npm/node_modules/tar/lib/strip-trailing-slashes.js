// this is the only approach that was significantly faster than using
// str.replace(/\/+$/, '') for strings ending with a lot of / chars and
// containing multiple / chars.
const batchStrings = [
  '/'.repeat(1024),
  '/'.repeat(512),
  '/'.repeat(256),
  '/'.repeat(128),
  '/'.repeat(64),
  '/'.repeat(32),
  '/'.repeat(16),
  '/'.repeat(8),
  '/'.repeat(4),
  '/'.repeat(2),
  '/',
]

module.exports = str => {
  for (const s of batchStrings) {
    while (str.length >= s.length && str.slice(-1 * s.length) === s)
      str = str.slice(0, -1 * s.length)
  }
  return str
}
