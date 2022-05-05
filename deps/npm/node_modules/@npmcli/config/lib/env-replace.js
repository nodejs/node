// replace any ${ENV} values with the appropriate environ.

const envExpr = /(?<!\\)(\\*)\$\{([^${}]+)\}/g

module.exports = (f, env) => f.replace(envExpr, (orig, esc, name) => {
  const val = env[name] !== undefined ? env[name] : `$\{${name}}`

  // consume the escape chars that are relevant.
  if (esc.length % 2) {
    return orig.slice((esc.length + 1) / 2)
  }

  return (esc.slice(esc.length / 2)) + val
})
