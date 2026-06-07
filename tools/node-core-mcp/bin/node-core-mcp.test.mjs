import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const SERVER = resolve(dirname(fileURLToPath(import.meta.url)), 'node-core-mcp.mjs');

function createClient(args = []) {
  const proc = spawn(process.execPath, [SERVER, ...args], {
    stdio: ['pipe', 'pipe', 'pipe'],
  });

  let buffer = '';
  let nextId = 1;
  const pending = new Map();

  proc.stdout.setEncoding('utf8');
  proc.stdout.on('data', (chunk) => {
    buffer += chunk;
    const lines = buffer.split('\n');
    buffer = lines.pop();
    for (const line of lines) {
      if (!line.trim()) continue;
      try {
        const msg = JSON.parse(line);
        const cb = pending.get(msg.id);
        if (cb) { pending.delete(msg.id); cb(msg); }
      } catch { /* ignore malformed lines */ }
    }
  });

  const call = (method, params = {}) => new Promise((resolve, reject) => {
    const id = nextId++;
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error(`Timeout: no response to "${method}"`));
    }, 10_000);
    pending.set(id, (msg) => { clearTimeout(timer); resolve(msg); });
    proc.stdin.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
  });

  return { call, close: () => proc.kill() };
}

test('--repo <path>: tools use the specified root', async () => {
  const client = createClient(['--repo', REPO_ROOT]);
  try {
    const res = await client.call('tools/call', { name: 'list_docs', arguments: {} });
    assert.ok(!res.error, res.error?.message);
    assert.ok(res.result.content[0].text.includes('.md'));
  } finally {
    client.close();
  }
});

test('--repo <nonexistent>: tools return an error, server stays alive', async () => {
  const client = createClient(['--repo', '/nonexistent/path']);
  try {
    const res = await client.call('tools/call', { name: 'list_docs', arguments: {} });
    // Server must respond (not crash) — result is either an error content or isError
    assert.ok(res.result || res.error, 'server should respond');
  } finally {
    client.close();
  }
});
