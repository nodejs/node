// replace any ${ENV} values with the appropriate environ.
// optional "?" modifier can be used like this: ${ENV?} so in case of the variable being not defined, it evaluates into empty string.

const envExpr = /(?<!\\)(\\*)\$\{([^${}?]+)(\?)?\}/g

module.exports = (f, env) => f.replace(envExpr, (orig, esc, name, modifier) => {
  const fallback = modifier === '?' ? '' : `$\{${name}}`
  const val = env[name] !== undefined ? env[name] : fallback

  // consume the escape chars that are relevant.
  if (esc.length % 2) {
    return orig.slice((esc.length + 1) / 2)
  }

  return (esc.slice(esc.length / 2)) + val
})
