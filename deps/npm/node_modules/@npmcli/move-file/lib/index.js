const { dirname, join, resolve, relative, isAbsolute } = require('path')
const rimraf_ = require('rimraf')
const { promisify } = require('util')
const {
  access: access_,
  accessSync,
  copyFile: copyFile_,
  copyFileSync,
  readdir: readdir_,
  readdirSync,
  rename: rename_,
  renameSync,
  stat: stat_,
  statSync,
  lstat: lstat_,
  lstatSync,
  symlink: symlink_,
  symlinkSync,
  readlink: readlink_,
  readlinkSync,
} = require('fs')

const access = promisify(access_)
const copyFile = promisify(copyFile_)
const readdir = promisify(readdir_)
const rename = promisify(rename_)
const stat = promisify(stat_)
const lstat = promisify(lstat_)
const symlink = promisify(symlink_)
const readlink = promisify(readlink_)
const rimraf = promisify(rimraf_)
const rimrafSync = rimraf_.sync

const mkdirp = require('mkdirp')

const pathExists = async path => {
  try {
    await access(path)
    return true
  } catch (er) {
    return er.code !== 'ENOENT'
  }
}

const pathExistsSync = path => {
  try {
    accessSync(path)
    return true
  } catch (er) {
    return er.code !== 'ENOENT'
  }
}

const moveFile = async (source, destination, options = {}, root = true, symlinks = []) => {
  if (!source || !destination) {
    throw new TypeError('`source` and `destination` file required')
  }

  options = {
    overwrite: true,
    ...options,
  }

  if (!options.overwrite && await pathExists(destination)) {
    throw new Error(`The destination file exists: ${destination}`)
  }

  await mkdirp(dirname(destination))

  try {
    await rename(source, destination)
  } catch (error) {
    if (error.code === 'EXDEV' || error.code === 'EPERM') {
      const sourceStat = await lstat(source)
      if (sourceStat.isDirectory()) {
        const files = await readdir(source)
        await Promise.all(files.map((file) =>
          moveFile(join(source, file), join(destination, file), options, false, symlinks)
        ))
      } else if (sourceStat.isSymbolicLink()) {
        symlinks.push({ source, destination })
      } else {
        await copyFile(source, destination)
      }
    } else {
      throw error
    }
  }

  if (root) {
    await Promise.all(symlinks.map(async ({ source: symSource, destination: symDestination }) => {
      let target = await readlink(symSource)
      // junction symlinks in windows will be absolute paths, so we need to
      // make sure they point to the symlink destination
      if (isAbsolute(target)) {
        target = resolve(symDestination, relative(symSource, target))
      }
      // try to determine what the actual file is so we can create the correct
      // type of symlink in windows
      let targetStat = 'file'
      try {
        targetStat = await stat(resolve(dirname(symSource), target))
        if (targetStat.isDirectory()) {
          targetStat = 'junction'
        }
      } catch {
        // targetStat remains 'file'
      }
      await symlink(
        target,
        symDestination,
        targetStat
      )
    }))
    await rimraf(source)
  }
}

const moveFileSync = (source, destination, options = {}, root = true, symlinks = []) => {
  if (!source || !destination) {
    throw new TypeError('`source` and `destination` file required')
  }

  options = {
    overwrite: true,
    ...options,
  }

  if (!options.overwrite && pathExistsSync(destination)) {
    throw new Error(`The destination file exists: ${destination}`)
  }

  mkdirp.sync(dirname(destination))

  try {
    renameSync(source, destination)
  } catch (error) {
    if (error.code === 'EXDEV' || error.code === 'EPERM') {
      const sourceStat = lstatSync(source)
      if (sourceStat.isDirectory()) {
        const files = readdirSync(source)
        for (const file of files) {
          moveFileSync(join(source, file), join(destination, file), options, false, symlinks)
        }
      } else if (sourceStat.isSymbolicLink()) {
        symlinks.push({ source, destination })
      } else {
        copyFileSync(source, destination)
      }
    } else {
      throw error
    }
  }

  if (root) {
    for (const { source: symSource, destination: symDestination } of symlinks) {
      let target = readlinkSync(symSource)
      // junction symlinks in windows will be absolute paths, so we need to
      // make sure they point to the symlink destination
      if (isAbsolute(target)) {
        target = resolve(symDestination, relative(symSource, target))
      }
      // try to determine what the actual file is so we can create the correct
      // type of symlink in windows
      let targetStat = 'file'
      try {
        targetStat = statSync(resolve(dirname(symSource), target))
        if (targetStat.isDirectory()) {
          targetStat = 'junction'
        }
      } catch {
        // targetStat remains 'file'
      }
      symlinkSync(
        target,
        symDestination,
        targetStat
      )
    }
    rimrafSync(source)
  }
}

module.exports = moveFile
module.exports.sync = moveFileSync
