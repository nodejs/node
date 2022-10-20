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

const { cwd } = process;
const console = require('internal/console/global');
const { createReadStream } = require('fs');
const { createServer } = require('http');
const { URL, pathToFileURL } = require('internal/url');
const { sep } = require('path');
const { setImmediate } = require('timers');

if (module.isPreloading) {
  process.env.NODE_REPL_EXTERNAL_MODULE = 'node:http/server';
  setImmediate(startServer);
}

function startServer() {
  const CWD_URL = pathToFileURL(cwd() + sep);

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
      '.css': 'text/css; charset=UTF-8',
      '.avif': 'image/avif',
      '.svg': 'image/svg+xml',
      '.jpg': 'image/jpeg',
      '.jpeg': 'image/jpeg',
      '.png': 'image/png',
      '.webp': 'image/webp',
      '.mp4': 'image/gif',
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
  }).listen(() => {
    console.log(`Server started on http://localhost:${server.address().port}`);
  });
}

module.exports = startServer;
