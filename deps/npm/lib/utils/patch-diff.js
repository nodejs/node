// Generate a git-compatible unified diff between two directories.
// Used by `npm patch commit` to capture edits against a clean tarball.
// The output is consumed by Arborist's apply step (jsdiff parsePatch).
const { createTwoFilesPatch } = require('diff')
const { readdir, readFile } = require('node:fs/promises')
const { join, sep } = require('node:path')

const IGNORE = new Set(['node_modules', '.git'])

// Recursively list file paths under dir, relative and posix-separated.
const listFiles = async dir => {
  const out = []
  const walk = async sub => {
    const entries = await readdir(join(dir, sub), { withFileTypes: true })
    for (const entry of entries) {
      const rel = sub ? `${sub}/${entry.name}` : entry.name
      if (entry.isDirectory()) {
        if (!IGNORE.has(entry.name)) {
          await walk(rel)
        }
      } else if (entry.isFile()) {
        out.push(rel)
      }
    }
  }
  await walk('')
  return out
}

const readMaybe = async file => {
  try {
    return await readFile(file, 'utf8')
  } catch {
    return null
  }
}

// Diff originalDir against editedDir, returning { diff, packageJsonChanged }.
// Added files use `--- /dev/null`, deleted files use `+++ /dev/null`.
// The root package.json is excluded: Arborist resolves the pre-patch manifest, so a patched manifest would apply to disk without being honored.
// ignore holds extra root-relative filenames the caller keeps out of the diff, e.g. the patch-update marker.
const diffDirs = async (originalDir, editedDir, ignore = new Set()) => {
  const [origFiles, editFiles] = await Promise.all([
    listFiles(originalDir),
    listFiles(editedDir),
  ])
  const all = [...new Set([...origFiles, ...editFiles])].sort()

  let result = ''
  let packageJsonChanged = false
  for (const file of all) {
    const native = file.split('/').join(sep)
    const [a, b] = await Promise.all([
      readMaybe(join(originalDir, native)),
      readMaybe(join(editedDir, native)),
    ])
    if (a === b) {
      continue
    }

    // the root package.json is never patchable; flag the change so commit can warn
    if (file === 'package.json') {
      packageJsonChanged = true
      continue
    }

    // caller-owned control files (e.g. the patch-update marker) never belong in the diff
    if (ignore.has(file)) {
      continue
    }

    let patch = createTwoFilesPatch(
      `a/${file}`, `b/${file}`, a || '', b || '', '', ''
    ).replace('===================================================================\n', '')

    // mark adds and deletes with /dev/null so the apply step creates/removes files
    if (a === null) {
      patch = patch.replace(`--- a/${file}\t`, '--- /dev/null\t')
    }
    if (b === null) {
      patch = patch.replace(`+++ b/${file}\t`, '+++ /dev/null\t')
    }
    result += patch
  }
  return { diff: result, packageJsonChanged }
}

module.exports = { diffDirs }
