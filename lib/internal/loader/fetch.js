'use strict';

const http = require('http');
const https = require('https');
const URL = require('url');
const zlib = require('zlib');

const requestTypeMap = {
  'http:': http,
  'https:': https,
};

function fetch(url) {
  return new Promise((resolve, reject) => {
    const opt = URL.parse(url);
    opt.method = 'GET';
    opt.headers = {
      'Accept-Encoding': 'gzlib, deflate',
    };
    const request = requestTypeMap[opt.protocol].get(url);

    let socket;

    const handleError = (err) => {
      const e = new Error(`Cannot find module: ${url}`);
      e.code = 'MODULE_NOT_FOUND';
      reject(e);
    };

    request.once('abort', handleError);
    request.once('error', handleError);
    request.once('socket', (s) => {
      socket = s;
      s.once('error', handleError);
    });

    request.once('response', (response) => {
      let stream = response;
      if (shouldUnzip(response)) {
        stream = response.pipe(zlib.createUnzip({
          flush: zlib.Z_SYNC_FLUSH,
          finishFlush: zlib.Z_SYNC_FLUSH,
        }));
      }

      if ([200, 201].includes(response.statusCode)) {
        let body = '';
        stream.on('data', (c) => { body += c; });

        stream.once('end', () => {
          if (socket)
            socket.removeListener('error', handleError);

          resolve(body);
        });
      } else if ([301, 302, 303, 307, 308].includes(response.statusCode)) {
        resolve(fetch(URL.resolve(url, response.headers.location)));
      } else {
        handleError();
      }
    });

    request.end();
  });
}

function shouldUnzip(res) {
  if (res.statusCode === 204 || res.statusCode === 304)
    return false;
  if (+res.headers['content-length'] === 0)
    return false;
  return /^\s*(?:deflate|gzip)\s*$/.test(res.headers['content-encoding']);
}

module.exports = fetch;
