const { Readable } = require('stream')
const { EnvHttpProxyAgent } = require('undici')
const { promises: fs } = require('graceful-fs')
const log = require('./log')

async function download (gyp, url) {
  log.http('GET', url)

  const requestOpts = {
    headers: {
      'User-Agent': `node-gyp v${gyp.version} (node ${process.version})`,
      Connection: 'keep-alive'
    },
    dispatcher: await createDispatcher(gyp)
  }

  let res
  try {
    res = await fetch(url, requestOpts)
  } catch (err) {
    // Built-in fetch wraps low-level errors in "TypeError: fetch failed" with
    // the underlying error on .cause. Callers inspect .code (e.g. ENOTFOUND).
    if (err.cause) {
      throw err.cause
    }
    throw err
  }

  log.http(res.status, res.url)

  const body = res.body ? Readable.fromWeb(res.body) : Readable.from([])
  return {
    status: res.status,
    url: res.url,
    body,
    text: async () => {
      let data = ''
      body.setEncoding('utf8')
      for await (const chunk of body) {
        data += chunk
      }
      return data
    }
  }
}

async function createDispatcher (gyp) {
  const env = process.env
  const hasProxyEnv = env.http_proxy || env.HTTP_PROXY || env.https_proxy || env.HTTPS_PROXY
  if (!gyp.opts.proxy && !gyp.opts.cafile && !hasProxyEnv) {
    return undefined
  }

  const opts = {}
  if (gyp.opts.cafile) {
    const ca = await readCAFile(gyp.opts.cafile)
    // EnvHttpProxyAgent forwards opts to both its internal Agent (direct) and
    // ProxyAgent (proxied). Agent reads TLS config from `connect`; ProxyAgent
    // reads it from `requestTls` (origin) / `proxyTls` (proxy). Set all three
    // so the custom CA is applied regardless of which path a request takes.
    opts.connect = { ca }
    opts.requestTls = { ca }
    opts.proxyTls = { ca }
  }
  if (gyp.opts.proxy) {
    opts.httpProxy = gyp.opts.proxy
    opts.httpsProxy = gyp.opts.proxy
  }
  if (gyp.opts.noproxy) {
    opts.noProxy = gyp.opts.noproxy
  }
  return new EnvHttpProxyAgent(opts)
}

async function readCAFile (filename) {
  // The CA file can contain multiple certificates so split on certificate
  // boundaries.  [\S\s]*? is used to match everything including newlines.
  const ca = await fs.readFile(filename, 'utf8')
  const re = /(-----BEGIN CERTIFICATE-----[\S\s]*?-----END CERTIFICATE-----)/g
  return ca.match(re)
}

module.exports = {
  download,
  readCAFile
}
