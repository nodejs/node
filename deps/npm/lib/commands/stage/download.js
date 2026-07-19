const { log, output, META } = require('proc-log')
const { writeFile } = require('node:fs/promises')
const { resolve } = require('node:path')
const tar = require('tar')
const npmFetch = require('npm-registry-fetch')
const { getContents, logTar } = require('../../utils/tar.js')
const { validateUUID } = require('../../utils/validate-uuid.js')
const BaseCommand = require('../../base-cmd.js')

class StageDownload extends BaseCommand {
  static description = 'Download the tarball of a staged package for inspection'
  static name = 'download'
  static usage = ['<stage-id>']
  static params = ['json', 'registry']
  static positionals = 1

  async exec (args) {
    if (!args[0]) {
      throw this.usageError('Missing required <stage-id>')
    }
    const stageId = args[0]
    validateUUID(stageId, 'stage-id')
    const opts = { ...this.npm.flatOptions }
    const unicode = this.npm.config.get('unicode')
    const json = this.npm.config.get('json')

    log.notice('', `Downloading staged package ${stageId}`)

    const res = await npmFetch(`/-/stage/${stageId}/tarball`, opts)
    const data = Buffer.from(await res.arrayBuffer())

    const manifest = await this.#readManifestFromTarball(data)
    const pkgContents = await getContents(manifest, data)
    logTar(pkgContents, { unicode, json, key: pkgContents.name })

    const safeName = pkgContents.name.replace('@', '').replace('/', '-')
    const filename = `${safeName}-${pkgContents.version}-${stageId}.tgz`
    const dest = resolve(process.cwd(), filename)

    await writeFile(dest, data)
    if (!json) {
      output.standard(filename, { [META]: true, redact: false })
    }
  }

  async #readManifestFromTarball (tarballData) {
    let manifestJson
    const stream = tar.t({
      onentry (entry) {
        if (entry.path === 'package/package.json') {
          const chunks = []
          entry.on('data', c => chunks.push(c))
          entry.on('end', () => {
            manifestJson = JSON.parse(Buffer.concat(chunks).toString())
          })
        } else {
          entry.resume()
        }
      },
    })
    // node-tar uses Minipass which processes synchronously on .end()
    stream.end(tarballData)
    if (!manifestJson) {
      throw new Error('Could not read package.json from tarball')
    }
    return manifestJson
  }
}

module.exports = StageDownload
