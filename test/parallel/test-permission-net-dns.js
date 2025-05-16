// Flags: --permission --allow-fs-read=*
'use strict';

const common = require('../common');
const assert = require('assert');
const dns = require('dns');
const { Resolver } = dns.promises;

{
  dns.lookup('localhost', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
  dns.promises.lookup('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}

{
  dns.resolve('localhost', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
  dns.resolve('come.on.fhqwhgads.test', (err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  });
  dns.promises.resolve('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}

{
  const resolver = new Resolver();
  resolver.resolve('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}

{
  const ip = '8.8.8.8';
  dns.reverse(ip, common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  dns.promises.reverse(ip).catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}

{
  dns.lookup('127.0.0.1', common.mustCall((err, address, family) => {
    assert.ifError(err);
    assert.strictEqual(typeof address, 'string');
    assert.ok(family === 4 || family === 6);
  }));

  dns.lookup('127.0.0.1', { family: 4 }, common.mustCall((err, address, family) => {
    assert.ifError(err);
    assert.strictEqual(typeof address, 'string');
    assert.strictEqual(family, 4);
  }));

  dns.lookup('127.0.0.1', { all: true }, common.mustCall((err, results) => {
    assert.ifError(err);
    assert(Array.isArray(results));
    assert(results.length > 0);
  }));
}

{
  dns.resolve4('localhost', common.mustCall((err, addresses) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));

  const resolver = new Resolver();
  resolver.setServers(['127.0.0.1']);

  resolver.resolve('localhost').catch(common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
  }));
}
