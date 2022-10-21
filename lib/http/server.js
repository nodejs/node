'use strict';

const {
  Promise,
  RegExpPrototypeExec,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeLastIndexOf,
  StringPrototypeReplace,
  StringPrototypeSlice,
  StringPrototypeStartsWith,
  StringPrototypeToLocaleLowerCase,
} = primordials;

const { cwd, nextTick } = process;
const console = require('internal/console/global');
const { createReadStream } = require('fs');
const { createServer } = require('http');
const { URL, pathToFileURL } = require('internal/url');
const { sep } = require('path');
const { emitExperimentalWarning } = require('internal/util');

emitExperimentalWarning('http/server');

if (module.isPreloading) {
  const { parseArgs } = require('internal/util/parse_args/parse_args');
  const { values: { port, host } } = parseArgs({ options: {
    port: {
      type: 'string',
      short: 'p',
    },
    host: {
      type: 'string',
      short: 'b',
      default: 'localhost',
    },
  } });
  nextTick(startServer, port, host, process.argv[1]);
  process.env.NODE_REPL_EXTERNAL_MODULE = 'node:http/server';
  process.argv[1] = 'node:http/server';
}

function startServer(port = undefined, host, directory = cwd()) {
  const CWD_URL = pathToFileURL(directory + sep);

  const server = createServer(async (req, res) => {
    const url = new URL(
      req.url === '/' || StringPrototypeStartsWith(req.url, '/?') ?
        './index.html' :
        StringPrototypeReplace(new URL(req.url, 'root://').toString(), 'root://', '.'),
      CWD_URL
    );

    const mime = {
      '__proto__': null,
      '.html': 'text/html; charset=UTF-8',
      '.js': 'text/javascript; charset=UTF-8',
      '.css': 'text/css; charset=UTF-8',
      '.avif': 'image/avif',
      '.svg': 'image/svg+xml',
      '.jpg': 'image/jpeg',
      '.jpeg': 'image/jpeg',
      '.png': 'image/png',
      '.webp': 'image/webp',
      '.woff2': 'font/woff2',
    };
    const ext = StringPrototypeToLocaleLowerCase(
      StringPrototypeSlice(url.pathname, StringPrototypeLastIndexOf(url.pathname, '.'))
    );
    if (ext in mime) res.setHeader('Content-Type', mime[ext]);

    try {
      try {
        const stream = createReadStream(url, { emitClose: false });
        await new Promise((resolve, reject) => {
          stream.on('error', reject);
          stream.on('close', resolve);
          stream.pipe(res);
        });
      } catch (err) {
        if (err?.code === 'EISDIR') {
          if (StringPrototypeEndsWith(req.url, '/') || RegExpPrototypeExec(/^[^?]+\/\?/, req.url) !== null) {
            const stream = createReadStream(new URL('./index.html', url), {
              emitClose: false,
            });
            await new Promise((resolve, reject) => {
              stream.on('error', reject);
              stream.on('close', resolve);
              stream.pipe(res);
            });
          } else {
            res.statusCode = 307;
            const index = StringPrototypeIndexOf(req.url, '?');
            res.setHeader(
              'Location',
              index === -1 ?
                `${req.url}/` :
                `${StringPrototypeSlice(req.url, 0, index)}/${StringPrototypeSlice(req.url, index)}`
            );
            res.end('Temporary Redirect');
          }
        } else {
          throw err;
        }
      }
    } catch (err) {
      if (err?.code === 'ENOENT') {
        console.log('404', req.url);
        res.statusCode = 404;
        res.end('Not Found');
      } else {
        console.error(`Error while loading ${req.url}:`);
        console.error(err);
        res.statusCode = 500;
        res.end('Internal Error');
      }
    }
  });
  const callback = () => {
    const { address, family, port } = server.address();
    const host = family === 'IPv6' ? `[${address}]` : address;
    console.log(`Server started on http://${host}:${port}`);
  };
  if (host != null) {
    server.listen(port, host, callback);
  } else if (port != null) {
    server.listen(port, callback);
  } else {
    server.listen(callback);
  }
}

module.exports = startServer;
