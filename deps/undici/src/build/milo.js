'use strict'

const { execFileSync } = require('node:child_process')
const { chmod, mkdtemp, readFile, cp, rm, mkdir } = require('node:fs/promises')
const { resolve } = require('node:path')

const artifacts = {
  'release/package-cjs/src/no-simd/index.js': 'no-simd.js',
  'release/package-cjs/src/simd/index.js': 'simd.js',
  'release/package-cjs/LICENSE.md': 'LICENSE.md'
}

function docker (...args) {
  return execFileSync('docker', args, { stdio: 'inherit' })
}

async function buildMilo () {
  // Setup some folders
  const source = resolve(__dirname, '../deps/milo')
  const destination = resolve(__dirname, '../lib/milo')
  const temporary = await mkdtemp(resolve(__dirname, '../deps/.build-'))
  const imageFile = resolve(temporary, 'id')

  try {
    await chmod(temporary, 0o777)
    docker('build', '--iidfile', imageFile, source)

    const image = (await readFile(imageFile, 'utf8')).trim()
    if (!/^sha256:[a-f0-9]{64}$/.test(image)) {
      throw new Error('Docker did not return a valid milo build image ID')
    }

    // Follow upstream's README: sources are read-only; only results are mounted writable.
    const sourceMount = `type=bind,source=${source},target=/src,readonly`
    const outputMount = `type=bind,source=${temporary},target=/output`
    docker('run', '--rm', '--mount', sourceMount, '--mount', outputMount, image)

    // Clean the destination directory and recreate it before copying artifacts.
    await rm(destination, { recursive: true, force: true })
    await mkdir(destination, { recursive: true })

    // Copy artifacts from the temporary build output to the destination directory.
    for (const [from, to] of Object.entries(artifacts)) {
      await cp(resolve(temporary, from), resolve(destination, to))
    }

    console.log(`Milo release artifacts saved in ${destination}`)
  } finally {
    await rm(temporary, { recursive: true, force: true })
  }
}

buildMilo()
