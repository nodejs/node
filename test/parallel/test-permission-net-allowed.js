// Flags: --permission --allow-net --allow-fs-read=*
'use strict';
const common = require('../common');

const { Resolver } = dns.promises;

// DNS
{
  dns.lookup('localhost', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
  dns.promises.lookup('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
  dns.resolve('localhost', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
  dns.resolve('come.on.fhqwhgads.test', (err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  });
  dns.promises.resolve('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  const resolver = new Resolver();
  resolver.resolve('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  const ip = '8.8.8.8';
  dns.reverse(ip, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  dns.promises.reverse(ip).catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}

// TODO: continue
