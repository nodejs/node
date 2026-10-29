const { output, META } = require('proc-log')
const npa = require('npm-package-arg')
const npmFetch = require('npm-registry-fetch')
const { logStageItem } = require('../../utils/key-values.js')
const BaseCommand = require('../../base-cmd.js')

class StageList extends BaseCommand {
  static description = 'List all staged package versions'
  static name = 'list'
  static usage = ['[<package-spec>]']
  static params = ['json', 'registry']

  async exec (args) {
    let packageFilter = null
    if (args[0]) {
      const spec = npa(args[0])
      if (spec.rawSpec !== '*') {
        throw this.usageError('Version specifiers are not supported for listing staged packages')
      }
      packageFilter = spec.name
    }
    const opts = { ...this.npm.flatOptions }
    const json = this.npm.config.get('json')

    const allItems = await this.#fetchAllPages(opts, packageFilter)

    if (json) {
      output.standard(JSON.stringify(allItems, null, 2), { [META]: true, redact: false })
      return
    }

    if (allItems.length === 0) {
      if (packageFilter) {
        output.standard(`No staged versions of package name "${packageFilter}".`)
      } else {
        output.standard('No staged packages found.')
      }
      return
    }

    for (let i = 0; i < allItems.length; i++) {
      if (i > 0) {
        output.standard('')
      }
      logStageItem(allItems[i], { chalk: this.npm.chalk })
    }
  }

  async #fetchAllPages (opts, packageFilter) {
    const items = []
    let page = 0
    const perPage = 100
    while (true) {
      const query = { page, perPage }
      if (packageFilter) {
        query.package = packageFilter
      }
      const res = await npmFetch.json('/-/stage', {
        ...opts,
        query,
      })
      items.push(...res.items)
      if (items.length >= res.total || res.items.length < perPage) {
        break
      }
      page++
    }
    return items
  }
}

module.exports = StageList
