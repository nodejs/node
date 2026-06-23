// Native dependency patching helpers shared across build-ideal-tree and reify.
// Patches are plain unified diffs (git apply-compatible) applied with jsdiff using a fuzz factor of 0 so that any context drift fails loudly.
const { applyPatch, parsePatch } = require('diff')
const ssri = require('ssri')
const fs = require('node:fs')
const { promises: fsp } = fs
const { resolve, relative, dirname, isAbsolute } = require('node:path')

// Compute the SSRI integrity of a patch file's contents.
// Accepts a string or Buffer and returns a sha512 SSRI string.
const patchIntegrity = data =>
  ssri.fromData(Buffer.isBuffer(data) ? data : Buffer.from(data, 'utf8'), {
    algorithms: ['sha512'],
  }).toString()

// Strip a leading git-style "a/" or "b/" prefix from a diff path.
const stripPrefix = file => file.replace(/^[ab]\//, '')

// True when a diff path points at /dev/null, signalling a file add or delete.
const isDevNull = file => !file || file === '/dev/null' || /(^|\/)\.dev\/null$/.test(file)

const patchError = (message, code, file) =>
  Object.assign(new Error(message), { code, file })

// Resolve a diff path under cwd and refuse anything that escapes the package directory.
const containedTarget = (cwd, file) => {
  const target = resolve(cwd, file)
  const rel = relative(cwd, target)
  if (!rel || rel.startsWith('..') || isAbsolute(rel)) {
    throw patchError(`patch path escapes the package directory: ${file}`, 'EPATCHUNSAFE', file)
  }
  return target
}

// Run a parsed file patch against a source string with fuzz 0.
// Returns the patched text, or throws EPATCHFAILED on any context mismatch.
const strictApply = (source, filePatch, file) => {
  const patched = applyPatch(source, filePatch, { fuzzFactor: 0 })
  if (patched === false) {
    throw patchError(`patch could not be applied to ${file}`, 'EPATCHFAILED', file)
  }
  return patched
}

// Apply a single parsed file patch under cwd.
// Handles modified, added (--- /dev/null) and deleted (+++ /dev/null) files.
const applyFilePatch = async (filePatch, cwd) => {
  const isAdd = isDevNull(filePatch.oldFileName)
  const isDelete = isDevNull(filePatch.newFileName)

  if (isDelete) {
    const file = stripPrefix(filePatch.oldFileName)
    const target = containedTarget(cwd, file)
    // verify the file still matches the diff before removing it
    const source = await fsp.readFile(target, 'utf8').catch(() => {
      throw patchError(`patch target to delete is missing: ${file}`, 'EPATCHFAILED', file)
    })
    strictApply(source, filePatch, file)
    await fsp.rm(target, { force: true })
    return
  }

  const file = stripPrefix(filePatch.newFileName)
  const target = containedTarget(cwd, file)

  if (isAdd) {
    // a new file must not already exist, otherwise the tarball drifted
    if (fs.existsSync(target)) {
      throw patchError(`patch adds a file that already exists: ${file}`, 'EPATCHFAILED', file)
    }
    const created = strictApply('', filePatch, file)
    await fsp.mkdir(dirname(target), { recursive: true })
    await fsp.writeFile(target, created)
    return
  }

  const source = await fsp.readFile(target, 'utf8').catch(() => {
    throw patchError(`patch target is missing: ${file}`, 'EPATCHFAILED', file)
  })
  const mode = (await fsp.stat(target)).mode
  const patched = strictApply(source, filePatch, file)
  await fsp.writeFile(target, patched)
  await fsp.chmod(target, mode)
}

// Apply a unified diff to the package extracted at `cwd`.
// `patch` is the raw diff contents (string or Buffer).
// Throws with code EPATCHFAILED on any hunk or file that cannot be applied.
const applyPatchToDir = async ({ patch, cwd }) => {
  const filePatches = parsePatch(patch.toString('utf8'))
  for (const filePatch of filePatches) {
    // jsdiff emits an empty trailing patch for some inputs; skip those.
    if (!filePatch.hunks.length && isDevNull(filePatch.oldFileName) && isDevNull(filePatch.newFileName)) {
      continue
    }
    try {
      await applyFilePatch(filePatch, cwd)
    } catch (er) {
      // re-code raw filesystem errors so a patch failure is never mistaken for an optional-install skip
      if (typeof er?.code === 'string' && er.code.startsWith('EPATCH')) {
        throw er
      }
      throw Object.assign(new Error(`failed to apply patch: ${er.message}`), { code: 'EPATCHFAILED', cause: er })
    }
  }
}

module.exports = {
  applyPatchToDir,
  patchIntegrity,
}
