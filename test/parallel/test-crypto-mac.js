'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const { EventEmitter } = require('events');
const assert = require('assert');
const crypto = require('crypto');

assert.throws(() => crypto.createMac('boom', 'secret'),
              /The value "boom" is invalid for option "mac"/);

assert.throws(() => crypto.createMac('cmac', 'secret'),
              /"options[.]cipher" property must be of type string/);

assert.throws(() => crypto.createMac('hmac', 'secret'),
              /"options[.]digest" property must be of type string/);

assert.throws(() => crypto.createMac('cmac', 'secret', { cipher: 'boom' }),
              /Unknown cipher/);

assert.throws(() => crypto.createMac('hmac', 'secret', { digest: 'boom' }),
              /Unknown message digest/);

// cmac
{
  const key = Buffer.from('77a77faf290c1fa30c683df16ba7a77b', 'hex');
  const data =
    Buffer.from(
      '020683e1f0392f4cac54318b6029259e9c553dbc4b6ad998e64d58e4e7dc2e13',
      'hex');
  const expected = Buffer.from('fbfea41bf9740cb501f1292c21cebb40', 'hex');
  const mac = crypto.createMac('cmac', key, { cipher: 'aes-128-cbc' });
  mac.update(data);
  assert.deepStrictEqual(mac.final(), expected);
  assert.deepStrictEqual(mac.final(), expected);  // Idempotent.
}

// hmac
{
  const expected =
    Buffer.from('1b2c16b75bd2a870c114153ccda5bcfc' +
                'a63314bc722fa160d690de133ccbb9db', 'hex');
  const mac = crypto.createMac('hmac', 'secret', { digest: 'sha256' });
  mac.update('data');
  assert.deepStrictEqual(mac.final(), expected);
  assert.deepStrictEqual(mac.final(), expected);  // Idempotent.
}

// poly1305
{
  const key =
    Buffer.from(
      '1c9240a5eb55d38af333888604f6b5f0473917c1402b80099dca5cbc207075c0',
      'hex');
  const data =
    Buffer.from(
      '2754776173206272696c6c69672c20616e642074686520736c6974687920' +
      '746f7665730a446964206779726520616e642067696d626c6520696e2074' +
      '686520776162653a0a416c6c206d696d737920776572652074686520626f' +
      '726f676f7665732c0a416e6420746865206d6f6d65207261746873206f75' +
      '7467726162652e',
      'hex');
  const expected = Buffer.from('4541669a7eaaee61e708dc7cbcc5eb62', 'hex');
  const mac = crypto.createMac('poly1305', key).update(data);
  assert.deepStrictEqual(mac.final(), expected);
  assert.deepStrictEqual(mac.final(), expected);  // Idempotent.
}

// siphash
{
  const key = Buffer.from('000102030405060708090A0B0C0D0E0F', 'hex');
  const data = Buffer.from('000102030405', 'hex');
  const expected = Buffer.from('14eeca338b208613485ea0308fd7a15e', 'hex');
  const mac = crypto.createMac('siphash', key).update(data);
  assert.deepStrictEqual(mac.final(), expected);
  assert.deepStrictEqual(mac.final(), expected);  // Idempotent.
}

// streaming mac
{
  const key =
    Buffer.from(
      '1c9240a5eb55d38af333888604f6b5f0473917c1402b80099dca5cbc207075c0',
      'hex');
  const data =
    Buffer.from(
      '2754776173206272696c6c69672c20616e642074686520736c6974687920' +
      '746f7665730a446964206779726520616e642067696d626c6520696e2074' +
      '686520776162653a0a416c6c206d696d737920776572652074686520626f' +
      '726f676f7665732c0a416e6420746865206d6f6d65207261746873206f75' +
      '7467726162652e',
      'hex');
  const expected = Buffer.from('4541669a7eaaee61e708dc7cbcc5eb62', 'hex');
  const mac = crypto.createMac('poly1305', key);

  const chunks = [];
  // eslint-disable-next-line new-parens
  mac.pipe(new class extends EventEmitter {
    write(chunk) { chunks.push(chunk); }
    end() {}
  });

  mac.write(data.slice(0, 1));
  for (let i = 1; i < data.length; i += i) mac.write(data.slice(i, i + i));
  mac.end();

  assert.deepStrictEqual(chunks, [expected]);
}
