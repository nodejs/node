import * as common from '../common/index.mjs';
import assert from 'node:assert';
import dgram from 'node:dgram';
import { describe, it } from 'node:test';

describe('dgram.Socket[Symbol.asyncDispose]()', () => {
  it('should close the socket', async () => {
    const server = dgram.createSocket({ type: 'udp4' });
    server.on('close', common.mustCall());
    await server[Symbol.asyncDispose]().then(common.mustCall());

    assert.throws(() => server.address(), { code: 'ERR_SOCKET_DGRAM_NOT_RUNNING' });
  });

  it('should resolve even if the socket is already closed', async () => {
    const server = dgram.createSocket({ type: 'udp4' });
    await server[Symbol.asyncDispose]().then(common.mustCall());
    await server[Symbol.asyncDispose]().then(common.mustCall(), common.mustNotCall());
  });
});
