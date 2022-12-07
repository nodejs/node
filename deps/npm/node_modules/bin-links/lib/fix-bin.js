// make sure that bins are executable, and that they don't have
// windows line-endings on the hashbang line.
const {
  chmod,
  open,
  readFile,
} = require('fs/promises')

const execMode = 0o777 & (~process.umask())

const writeFileAtomic = require('write-file-atomic')

const isWindowsHashBang = buf =>
  buf[0] === '#'.charCodeAt(0) &&
  buf[1] === '!'.charCodeAt(0) &&
  /^#![^\n]+\r\n/.test(buf.toString())

const isWindowsHashbangFile = file => {
  const FALSE = () => false
  return open(file, 'r').then(fh => {
    const buf = Buffer.alloc(2048)
    return fh.read(buf, 0, 2048, 0)
      .then(
        () => {
          const isWHB = isWindowsHashBang(buf)
          return fh.close().then(() => isWHB, () => isWHB)
        },
        // don't leak FD if read() fails
        () => fh.close().then(FALSE, FALSE)
      )
  }, FALSE)
}

const dos2Unix = file =>
  readFile(file, 'utf8').then(content =>
    writeFileAtomic(file, content.replace(/^(#![^\n]+)\r\n/, '$1\n')))

const fixBin = (file, mode = execMode) => chmod(file, mode)
  .then(() => isWindowsHashbangFile(file))
  .then(isWHB => isWHB ? dos2Unix(file) : null)

module.exports = fixBin
