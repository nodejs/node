// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  BlockList,
  SocketAddress,
} = require('net');
const { inspect } = require('util');

const {
  InternalSocketAddress,
} = require('internal/socketaddress');
const { internalBinding } = require('internal/test/binding');
const {
  SocketAddress: _SocketAddress,
  AF_INET,
} = internalBinding('block_list');

const { describe, it } = require('node:test');

describe('net.SocketAddress...', () => {

  it('is cloneable', () => {
    const sa = new SocketAddress();
    assert.strictEqual(sa.address, '127.0.0.1');
    assert.strictEqual(sa.port, 0);
    assert.strictEqual(sa.family, 'ipv4');
    assert.strictEqual(sa.flowlabel, 0);

    const mc = new MessageChannel();
    mc.port1.onmessage = common.mustCall(({ data }) => {
      assert.ok(SocketAddress.isSocketAddress(data));

      assert.strictEqual(data.address, '127.0.0.1');
      assert.strictEqual(data.port, 0);
      assert.strictEqual(data.family, 'ipv4');
      assert.strictEqual(data.flowlabel, 0);

      mc.port1.close();
    });
    mc.port2.postMessage(sa);
  });

  it('has reasonable defaults', () => {
    const sa = new SocketAddress({});
    assert.strictEqual(sa.address, '127.0.0.1');
    assert.strictEqual(sa.port, 0);
    assert.strictEqual(sa.family, 'ipv4');
    assert.strictEqual(sa.flowlabel, 0);
  });

  it('interprets simple ipv4 correctly', () => {
    const sa = new SocketAddress({
      address: '123.123.123.123',
    });
    assert.strictEqual(sa.address, '123.123.123.123');
    assert.strictEqual(sa.port, 0);
    assert.strictEqual(sa.family, 'ipv4');
    assert.strictEqual(sa.flowlabel, 0);
  });

  it('sets the port correctly', () => {
    const sa = new SocketAddress({
      address: '123.123.123.123',
      port: 80
    });
    assert.strictEqual(sa.address, '123.123.123.123');
    assert.strictEqual(sa.port, 80);
    assert.strictEqual(sa.family, 'ipv4');
    assert.strictEqual(sa.flowlabel, 0);
  });

  it('interprets simple ipv6 correctly', () => {
    const sa = new SocketAddress({
      family: 'ipv6'
    });
    assert.strictEqual(sa.address, '::');
    assert.strictEqual(sa.port, 0);
    assert.strictEqual(sa.family, 'ipv6');
    assert.strictEqual(sa.flowlabel, 0);
  });

  it('uses the flowlabel correctly', () => {
    const sa = new SocketAddress({
      family: 'ipv6',
      flowlabel: 1,
    });
    assert.strictEqual(sa.address, '::');
    assert.strictEqual(sa.port, 0);
    assert.strictEqual(sa.family, 'ipv6');
    assert.strictEqual(sa.flowlabel, 1);
  });

  it('validates input correctly', () => {
    [1, false, 'hello'].forEach((i) => {
      assert.throws(() => new SocketAddress(i), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });

    [1, false, {}, [], 'test'].forEach((family) => {
      assert.throws(() => new SocketAddress({ family }), {
        code: 'ERR_INVALID_ARG_VALUE'
      });
    });

    [1, false, {}, []].forEach((address) => {
      assert.throws(() => new SocketAddress({ address }), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    });

    [-1, false, {}, []].forEach((port) => {
      assert.throws(() => new SocketAddress({ port }), {
        code: 'ERR_SOCKET_BAD_PORT'
      });
    });

    assert.throws(() => new SocketAddress({ flowlabel: -1 }), {
      code: 'ERR_OUT_OF_RANGE'
    });
  });

  it('InternalSocketAddress correctly inherits from SocketAddress', () => {
    // Test that the internal helper class InternalSocketAddress correctly
    // inherits from SocketAddress and that it does not throw when its properties
    // are accessed.

    const address = '127.0.0.1';
    const port = 8080;
    const flowlabel = 0;
    const handle = new _SocketAddress(address, port, AF_INET, flowlabel);
    const addr = new InternalSocketAddress(handle);
    assert.ok(addr instanceof SocketAddress);
    assert.strictEqual(addr.address, address);
    assert.strictEqual(addr.port, port);
    assert.strictEqual(addr.family, 'ipv4');
    assert.strictEqual(addr.flowlabel, flowlabel);
  });

  it('SocketAddress.parse() works as expected', () => {
    // The exhaustive grammar table lives in test/cctest/test_sockaddr.cc,
    // next to the parser. These cover the JS layer and a few representatives.
    const good = [
      { input: '1.2.3.4', address: '1.2.3.4', port: 0, family: 'ipv4' },
      { input: '1.2.3.4:8080', address: '1.2.3.4', port: 8080, family: 'ipv4' },
      // A well known port must survive; the URL based parser dropped it.
      { input: '1.2.3.4:80', address: '1.2.3.4', port: 80, family: 'ipv4' },
      { input: '[1:0::]', address: '1::', port: 0, family: 'ipv6' },
      { input: '[1::8]:123', address: '1::8', port: 123, family: 'ipv6' },
      { input: '[::ffff:1.2.3.4]:80', address: '::ffff:1.2.3.4', port: 80, family: 'ipv6' },
      // A numeric IPv6 zone id is accepted and does not appear in the address.
      { input: '[fe80::1%2]:80', address: 'fe80::1', port: 80, family: 'ipv6' },
    ];

    good.forEach(({ input, ...expected }) => {
      const addr = SocketAddress.parse(input);
      assert.ok(addr, `${input} did not parse`);
      assert.deepStrictEqual(
        { address: addr.address, port: addr.port, family: addr.family },
        expected);
    });

    const bad = [
      // Legacy IPv4 forms. See CVE-2021-29923 and CVE-2021-29922.
      '0177.0.0.1:80',
      '0x7f.0.0.1:80',
      '2130706433:80',
      '127.1:80',
      '192.168.257:1',
      '0xffffffff',
      // URL syntax.
      'user@1.2.3.4:80',
      '1.2.3.4:80/foo',
      // Strings that only reach the parser through Utf8Value.
      '\u2460\u2461\u2462.4.5.6:80',
      '1.2.3.4\u0000junk:80',
      '1.2.3.4:80\u0000junk',
      // Representative structural rejections.
      '1.2.3.4:',
      '1.2.3.4:65536',
      '::1',
      '[fe80::1%lo0]:80',
      '',
      'localhost:80',
    ];

    bad.forEach((i) => {
      assert.strictEqual(SocketAddress.parse(i), undefined, `${i} should not parse`);
    });

    assert.throws(() => SocketAddress.parse(1), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  });

  it('SocketAddress.parse() returns a branded SocketAddress', () => {
    const parsed = SocketAddress.parse('1.2.3.4:8080');

    assert.ok(parsed instanceof SocketAddress);
    assert.ok(SocketAddress.isSocketAddress(parsed));
    assert.strictEqual(parsed.constructor, SocketAddress);
  });

  it('SocketAddress.parse() matches the constructor', () => {
    const parsed = SocketAddress.parse('1.2.3.4:8080');
    const built = new SocketAddress({ address: '1.2.3.4', port: 8080 });

    assert.deepStrictEqual(parsed.toJSON(), built.toJSON());
    assert.strictEqual(inspect(parsed), inspect(built));
  });

  it('SocketAddress.parse() returns a cloneable SocketAddress', () => {
    const parsed = SocketAddress.parse('1.2.3.4:8080');
    const clone = structuredClone(parsed);

    assert.ok(clone instanceof SocketAddress);
    assert.deepStrictEqual(clone.toJSON(), parsed.toJSON());
  });

  it('SocketAddress.parse() returns a SocketAddress BlockList accepts', () => {
    const list = new BlockList();
    list.addAddress(SocketAddress.parse('1.2.3.4:8080'));

    assert.ok(list.check('1.2.3.4'));
    assert.ok(!list.check('1.2.3.5'));
  });

});
