// Flags: --experimental-network-imports --dns-result-order=ipv4first
import * as common from '../common/index.mjs';
import { path, readKey } from '../common/fixtures.mjs';
import { pathToFileURL } from 'url';
import assert from 'assert';
import http from 'http';
import os from 'os';
import util from 'util';

if (!common.hasCrypto) {
  common.skip('missing crypto');
}

const https = (await import('https')).default;

const createHTTPServer = http.createServer;

// Needed to deal w/ test certs
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
const options = {
  key: readKey('agent1-key.pem'),
  cert: readKey('agent1-cert.pem')
};

const createHTTPSServer = https.createServer.bind(null, options);


const testListeningOptions = [
  {
    hostname: 'localhost',
    listenOptions: {
      host: '127.0.0.1'
    }
  },
];

const internalInterfaces = Object.values(os.networkInterfaces()).flat().filter(
  (iface) => iface?.internal && iface.address && !iface.scopeid
);
for (const iface of internalInterfaces) {
  testListeningOptions.push({
    hostname: iface?.family === 'IPv6' ? `[${iface?.address}]` : iface?.address,
    listenOptions: {
      host: iface?.address,
      ipv6Only: iface?.family === 'IPv6'
    }
  });
}

for (const { protocol, createServer } of [
  { protocol: 'http:', createServer: createHTTPServer },
  { protocol: 'https:', createServer: createHTTPSServer },
]) {
  const body = `
    export default (a) => () => a;
    export let url = import.meta.url;
  `;

  const base = 'http://127.0.0.1';
  for (const { hostname, listenOptions } of testListeningOptions) {
    const host = new URL(base);
    host.protocol = protocol;
    host.hostname = hostname;
    // /not-found is a 404
    // ?redirect causes a redirect, no body. JSON.parse({status:number,location:string})
    // ?mime sets the content-type, string
    // ?body sets the body, string
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
    await listen({
      ...listenOptions,
      port: 0
    });
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

    // Redirects have the same import.meta.url but different cache
    // entry on Web
    const relativeAfterRedirect = new URL(url.href + 'foo/index.js');
    const redirected = new URL(url.href + 'bar/index.js');
    redirected.searchParams.set('body', 'export let relativeDepURL = (await import("./baz.js")).url');
    relativeAfterRedirect.searchParams.set('redirect', JSON.stringify({
      status: 302,
      location: redirected.href
    }));
    const relativeAfterRedirectedNS = await import(relativeAfterRedirect.href);
    assert.strict.equal(
      relativeAfterRedirectedNS.relativeDepURL,
      url.href + 'bar/baz.js'
    );

    const crossProtocolRedirect = new URL(url.href);
    crossProtocolRedirect.searchParams.set('redirect', JSON.stringify({
      status: 302,
      location: 'data:text/javascript,'
    }));
    await assert.rejects(
      import(crossProtocolRedirect.href),
      { code: 'ERR_NETWORK_IMPORT_DISALLOWED' }
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

    const relativeDeps = new URL(url.href);
    relativeDeps.searchParams.set('body', `
      import * as http from "./";
      export {http};
    `);
    const relativeDepsNS = await import(relativeDeps.href);
    assert.strict.deepStrictEqual(Object.keys(relativeDepsNS), ['http']);
    assert.strict.equal(relativeDepsNS.http, ns);
    const fileDep = new URL(url.href);
    const { href } = pathToFileURL(path('/es-modules/message.mjs'));
    fileDep.searchParams.set('body', `
      import ${JSON.stringify(href)};
      export default 1;`);
    await assert.rejects(
      import(fileDep.href),
      { code: 'ERR_INVALID_URL_SCHEME' }
    );

    const builtinDep = new URL(url.href);
    builtinDep.searchParams.set('body', `
      import 'node:fs';
      export default 1;
    `);
    await assert.rejects(
      import(builtinDep.href),
      { code: 'ERR_INVALID_URL_SCHEME' }
    );

    const unprefixedBuiltinDep = new URL(url.href);
    unprefixedBuiltinDep.searchParams.set('body', `
      import 'fs';
      export default 1;
    `);
    await assert.rejects(
      import(unprefixedBuiltinDep.href),
      { code: 'ERR_INVALID_URL_SCHEME' }
    );

    const unsupportedMIME = new URL(url.href);
    unsupportedMIME.searchParams.set('mime', 'application/node');
    unsupportedMIME.searchParams.set('body', '');
    await assert.rejects(
      import(unsupportedMIME.href),
      { code: 'ERR_UNKNOWN_MODULE_FORMAT' }
    );

    server.close();
  }
}
