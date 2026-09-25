import * as common from '../common/index.mjs';
import assert from 'node:assert';
import net from 'node:net';
import { describe, it } from 'node:test';

describe('net.Server[Symbol.asyncDispose]()', () => {
  it('should close the server', async () => {
    const server = net.createServer();
    const timeoutRef = setTimeout(common.mustNotCall(), 2 ** 31 - 1);

    server.listen(0, common.mustCall(async () => {
      await server[Symbol.asyncDispose]().then(common.mustCall());
      assert.strictEqual(server.address(), null);
      clearTimeout(timeoutRef);
    }));

    server.on('close', common.mustCall());
  });

  it('should resolve even if the server is already closed', async () => {
    const server = net.createServer();
    const timeoutRef = setTimeout(common.mustNotCall(), 2 ** 31 - 1);

    server.listen(0, common.mustCall(async () => {
      await server[Symbol.asyncDispose]().then(common.mustCall());
      await server[Symbol.asyncDispose]().then(common.mustCall(), common.mustNotCall());
      clearTimeout(timeoutRef);
    }));
  });
});
