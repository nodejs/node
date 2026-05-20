const updateScripts = ({ content, originalContent = {} }) => {
  const newScripts = content.scripts

  if (!newScripts) {
    return originalContent
  }

  // validate scripts content being appended
  const hasInvalidScripts = () =>
    Object.entries(newScripts)
      .some(([key, value]) =>
        typeof key !== 'string' || typeof value !== 'string')
  if (hasInvalidScripts()) {
    throw Object.assign(
      new TypeError(
        'package.json scripts should be a key-value pair of strings.'),
      { code: 'ESCRIPTSINVALID' }
    )
  }

  return {
    ...originalContent,
    scripts: {
      ...newScripts,
    },
  }
}

module.exports = updateScripts
