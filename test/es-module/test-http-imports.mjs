// Flags: --experimental-https-modules
import '../common/index.mjs';
import { path, readKey } from '../common/fixtures.mjs';
import { pathToFileURL } from 'url';
import assert from 'assert';
import http from 'http';
import https from 'https';
import util from 'util';

const createHTTPServer = http.createServer;

// Needed to deal w/ test certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
const options = {
  key: readKey('agent1-key.pem'),
  cert: readKey('agent1-cert.pem')
};
const createHTTPSServer = https.createServer.bind(null, options);

for (const { protocol, createServer } of [
  { protocol: 'http:', createServer: createHTTPServer },
  { protocol: 'https:', createServer: createHTTPSServer },
]) {
  const body = `
    export default (a) => () => a;
    export let url = import.meta.url;
  `;

  const base = 'http://127.0.0.1';
  for (const { hostname, ip } of [
    { hostname: 'localhost', ip: '127.0.0.1' },
    { hostname: '127.0.0.1', ip: '127.0.0.1' },
    { hostname: '[::1]', ip: '::1' },
  ]) {
    const host = new URL(base);
    host.protocol = protocol;
    host.hostname = hostname;
    const server = createServer(function(_req, res) {
      const url = new URL(_req.url, host);
      const redirect = url.searchParams.get('redirect');
      if (redirect) {
        const { status, location } = JSON.parse(redirect);
        res.writeHead(status, {
          location
        });
        res.end();
        return;
      }
      res.writeHead(200, {
        'content-type': url.searchParams.get('mime') || 'text/javascript'
      });
      res.end(url.searchParams.get('body') || body);
    });

    const listen = util.promisify(server.listen.bind(server));
    await listen(0, ip);
    const url = new URL(host);
    url.port = server?.address()?.port;
    const ns = await import(url.href);
    assert.strict.deepStrictEqual(Object.keys(ns), ['default', 'url']);
    const obj = {};
    assert.strict.equal(ns.default(obj)(), obj);
    assert.strict.equal(ns.url, url.href);

    // Redirects have same import.meta.url but different cache
    // entry on Web
    const redirect = new URL(url.href);
    redirect.searchParams.set('redirect', JSON.stringify({
      status: 302,
      location: url.href
    }));
    const redirectedNS = await import(redirect.href);
    assert.strict.deepStrictEqual(
      Object.keys(redirectedNS),
      ['default', 'url']
    );
    assert.strict.notEqual(redirectedNS.default, ns.default);
    assert.strict.equal(redirectedNS.url, url.href);

    const crossProtocolRedirect = new URL(url.href);
    crossProtocolRedirect.searchParams.set('redirect', JSON.stringify({
      status: 302,
      location: 'data:text/javascript,'
    }));
    await assert.rejects(
      import(crossProtocolRedirect.href),
      'should not be able to redirect across protocols'
    );

    const deps = new URL(url.href);
    deps.searchParams.set('body', `
      export {data} from 'data:text/javascript,export let data = 1';
      import * as http from ${JSON.stringify(url.href)};
      export {http};
    `);
    const depsNS = await import(deps.href);
    assert.strict.deepStrictEqual(Object.keys(depsNS), ['data', 'http']);
    assert.strict.equal(depsNS.data, 1);
    assert.strict.equal(depsNS.http, ns);

    const fileDep = new URL(url.href);
    const { href } = pathToFileURL(path('/es-modules/message.mjs'));
    fileDep.searchParams.set('body', `
      import ${JSON.stringify(href)};
      export default 1;`);
    await assert.rejects(
      import(fileDep.href),
      'should not be able to load file: from http:');

    const builtinDep = new URL(url.href);
    builtinDep.searchParams.set('body', `
      import 'node:fs';
      export default 1;
    `);
    await assert.rejects(
      import(builtinDep.href),
      'should not be able to load node: from http:'
    );

    const unprefixedBuiltinDep = new URL(url.href);
    unprefixedBuiltinDep.searchParams.set('body', `
      import 'fs';
      export default 1;
    `);
    await assert.rejects(
      import(unprefixedBuiltinDep.href),
      'should not be able to load unprefixed builtins from http:'
    );

    const unsupportedMIME = new URL(url.href);
    unsupportedMIME.searchParams.set('mime', 'application/node');
    unsupportedMIME.searchParams.set('body', '');
    await assert.rejects(
      import(unsupportedMIME.href).catch(e => {
        console.error(e);
        throw e;
      }),
      'should not be able to load unsupported MIMEs from http:'
    );

    server.close();
  }
}
