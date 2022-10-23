'use strict';

const {
  ArrayPrototypeIncludes,
  Promise,
  RegExpPrototypeExec,
  StringPrototypeEndsWith,
  StringPrototypeIncludes,
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
const { Server } = require('_http_server');
const { URL, isURLInstance, pathToFileURL } = require('internal/url');
const { sep } = require('path');
const { emitExperimentalWarning, kEmptyObject } = require('internal/util');
const { getOptionValue } = require('internal/options');
const { validateFunction, validatePort } = require('internal/validators');

emitExperimentalWarning('http/server');

if (module.isPreloading) {
  const requiredModules = getOptionValue('--require');
  if (ArrayPrototypeIncludes(requiredModules, `node:${module.id}`) ||
      ArrayPrototypeIncludes(requiredModules, module.id)) {
    const { parseArgs } = require('internal/util/parse_args/parse_args');
    const { values: { port, host } } = parseArgs({ options: {
      port: {
        type: 'string',
        short: 'p',
      },
      host: {
        type: 'string',
        short: 'h',
      },
    } });
    nextTick(createStaticServer, { directory: process.argv[1], port, host });
    process.env.NODE_REPL_EXTERNAL_MODULE = 'node:http/server';
    process.argv[1] = 'node:http/server';
  }
}

function filterDotFiles(url) {
  return !StringPrototypeIncludes(url.pathname, '/.');
}

function createStaticServer(options = kEmptyObject) {
  const {
    directory = cwd(),
    port,
    host = 'localhost',
    mimeOverrides,
    filter = filterDotFiles,
  } = options;
  const directoryURL = isURLInstance(directory) ? directory : pathToFileURL(directory);
  // To be used as a base URL, it is necessary that the URL ends with a slash:
  const baseDirectoryURL = StringPrototypeEndsWith(directoryURL.pathname, '/') ?
    directoryURL : new URL(`${directoryURL}/`);

  if (port != null) validatePort(port);
  if (filter != null) validateFunction(filter, 'options.filter');

  const server = new Server(async (req, res) => {
    const url = new URL(
      req.url === '/' || StringPrototypeStartsWith(req.url, '/?') ?
        './index.html' :
        StringPrototypeReplace(new URL(req.url, 'root://').toString(), 'root://', '.'),
      baseDirectoryURL
    );

    if (filter != null && !filter(url)) {
      console.log('401', req.url);
      res.statusCode = 401;
      res.end('Not Authorized');
      return;
    }

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
      ...mimeOverrides,
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

  return server;
}

module.exports = createStaticServer;
