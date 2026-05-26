const { output, META } = require('proc-log')

const defaultPredicate = (key, value, chalk) => {
  if (value === null || value === undefined) {
    return null
  }
  return chalk.green(value)
}

function logObject (values, { chalk, json, predicate = defaultPredicate }) {
  if (json) {
    output.standard(JSON.stringify(values, null, 2), { [META]: true, redact: false })
    return
  }

  const lines = []
  for (const [key, value] of Object.entries(values)) {
    const formatted = predicate(key, value, chalk)
    if (formatted !== null) {
      lines.push(`${chalk.cyan(key)}: ${formatted}`)
    }
  }
  if (lines.length) {
    output.standard(lines.join('\n'), { [META]: true, redact: false })
  }
}

function logStageItem (item, { chalk }) {
  const { id, packageName, version, tag, createdAt, actor, actorType, shasum, ...rest } = item
  logObject({
    id,
    'package name': packageName,
    version,
    tag,
    'date staged': createdAt,
    'staged by': actorType ? `${actor} (${actorType})` : actor,
    shasum,
    ...rest,
  }, { chalk })
}

module.exports = { logObject, logStageItem, defaultPredicate }
