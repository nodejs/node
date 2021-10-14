// make sure that bins are executable, and that they don't have
// windows line-endings on the hashbang line.
const fs = require('fs')
const { promisify } = require('util')

const execMode = 0o777 & (~process.umask())

const writeFileAtomic = require('write-file-atomic')
const open = promisify(fs.open)
const close = promisify(fs.close)
const read = promisify(fs.read)
const chmod = promisify(fs.chmod)
const readFile = promisify(fs.readFile)

const isWindowsHashBang = buf =>
  buf[0] === '#'.charCodeAt(0) &&
  buf[1] === '!'.charCodeAt(0) &&
  /^#![^\n]+\r\n/.test(buf.toString())

const isWindowsHashbangFile = file => {
  const FALSE = () => false
  return open(file, 'r').then(fd => {
    const buf = Buffer.alloc(2048)
    return read(fd, buf, 0, 2048, 0)
      .then(
        () => {
          const isWHB = isWindowsHashBang(buf)
          return close(fd).then(() => isWHB, () => isWHB)
        },
        // don't leak FD if read() fails
        () => close(fd).then(FALSE, FALSE)
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
