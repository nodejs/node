import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const SERVER = resolve(dirname(fileURLToPath(import.meta.url)), '..', 'bin', 'node-core-mcp.mjs');

function createClient() {
  const proc = spawn(process.execPath, [SERVER, '--repo', REPO_ROOT], {
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

test('initialize returns protocol version and server info', async () => {
  const client = createClient();
  try {
    const res = await client.call('initialize', {
      protocolVersion: '2024-11-05',
      capabilities: {},
      clientInfo: { name: 'test', version: '0.1' },
    });
    assert.strictEqual(res.result.protocolVersion, '2024-11-05');
    assert.strictEqual(res.result.serverInfo.name, 'node-core');
    assert.ok(res.result.capabilities.tools);
  } finally {
    client.close();
  }
});

test('unknown method returns error -32601', async () => {
  const client = createClient();
  try {
    const res = await client.call('no/such/method');
    assert.ok(res.error);
    assert.strictEqual(res.error.code, -32601);
  } finally {
    client.close();
  }
});

test('tools/list returns all expected tools in order', async () => {
  const client = createClient();
  try {
    const res = await client.call('tools/list');
    const names = res.result.tools.map((t) => t.name);
    assert.deepStrictEqual(names, [
      'configure', 'build', 'run_test', 'run_tests',
      'search_code', 'list_docs', 'read_doc',
      'find_subsystem', 'list_relevant_tests', 'explain_test_failure', 'search_docs',
      'get_pr_metadata',
    ]);
  } finally {
    client.close();
  }
});

test('each tool has name, description, and inputSchema', async () => {
  const client = createClient();
  try {
    const res = await client.call('tools/list');
    for (const tool of res.result.tools) {
      assert.ok(tool.name, 'missing name');
      assert.ok(tool.description, `${tool.name}: missing description`);
      assert.ok(tool.inputSchema, `${tool.name}: missing inputSchema`);
    }
  } finally {
    client.close();
  }
});

test('tools/call unknown tool returns error -32601', async () => {
  const client = createClient();
  try {
    const res = await client.call('tools/call', { name: 'nonexistent', arguments: {} });
    assert.ok(res.error);
    assert.strictEqual(res.error.code, -32601);
  } finally {
    client.close();
  }
});
