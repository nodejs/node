'use strict'

// tar -x
const hlo = require('./high-level-opt.js')
const Unpack = require('./unpack.js')
const fs = require('fs')
const path = require('path')

const x = module.exports = (opt_, files, cb) => {
  if (typeof opt_ === 'function')
    cb = opt_, files = null, opt_ = {}
  else if (Array.isArray(opt_))
    files = opt_, opt_ = {}

  if (typeof files === 'function')
    cb = files, files = null

  if (!files)
    files = []
  else
    files = Array.from(files)

  const opt = hlo(opt_)

  if (opt.sync && typeof cb === 'function')
    throw new TypeError('callback not supported for sync tar functions')

  if (!opt.file && typeof cb === 'function')
    throw new TypeError('callback only supported with file option')

  if (files.length)
    filesFilter(opt, files)

  return opt.file && opt.sync ? extractFileSync(opt)
    : opt.file ? extractFile(opt, cb)
    : opt.sync ? extractSync(opt)
    : extract(opt)
}

// construct a filter that limits the file entries listed
// include child entries if a dir is included
const filesFilter = (opt, files) => {
  const map = new Map(files.map(f => [f.replace(/\/+$/, ''), true]))
  const filter = opt.filter

  const mapHas = (file, r) => {
    const root = r || path.parse(file).root || '.'
    const ret = file === root ? false
      : map.has(file) ? map.get(file)
      : mapHas(path.dirname(file), root)

    map.set(file, ret)
    return ret
  }

  opt.filter = filter
    ? (file, entry) => filter(file, entry) && mapHas(file.replace(/\/+$/, ''))
    : file => mapHas(file.replace(/\/+$/, ''))
}

const extractFileSync = opt => {
  const u = new Unpack.Sync(opt)

  const file = opt.file
  let threw = true
  let fd
  try {
    const stat = fs.statSync(file)
    const readSize = opt.maxReadSize || 16*1024*1024
    if (stat.size < readSize)
      u.end(fs.readFileSync(file))
    else {
      let pos = 0
      const buf = Buffer.allocUnsafe(readSize)
      fd = fs.openSync(file, 'r')
      while (pos < stat.size) {
        let bytesRead = fs.readSync(fd, buf, 0, readSize, pos)
        pos += bytesRead
        u.write(buf.slice(0, bytesRead))
      }
      u.end()
      fs.closeSync(fd)
    }
    threw = false
  } finally {
    if (threw && fd)
      try { fs.closeSync(fd) } catch (er) {}
  }
}

const extractFile = (opt, cb) => {
  const u = new Unpack(opt)
  const readSize = opt.maxReadSize || 16*1024*1024

  const file = opt.file
  const p = new Promise((resolve, reject) => {
    u.on('error', reject)
    u.on('close', resolve)

    fs.stat(file, (er, stat) => {
      if (er)
        reject(er)
      else if (stat.size < readSize)
        fs.readFile(file, (er, data) => {
          if (er)
            return reject(er)
          u.end(data)
        })
      else {
        const stream = fs.createReadStream(file, {
          highWaterMark: readSize
        })
        stream.on('error', reject)
        stream.pipe(u)
      }
    })
  })
  return cb ? p.then(cb, cb) : p
}

const extractSync = opt => {
  return new Unpack.Sync(opt)
}

const extract = opt => {
  return new Unpack(opt)
}
