'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn } = require('child_process');
const { once } = require('events');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');

// Refs: https://github.com/nodejs/node/issues/66440

function stty(args) {
  const { child: { stdout } } = spawnSyncAndExitWithoutError('stty', args, {
    encoding: 'utf8',
    stdio: ['inherit', 'pipe', 'inherit'],
  });
  return stdout.trim();
}

const childScript = [
  'process.channel.ref();',
  'process.once("message", (exit) => {',
  '  process.disconnect();',
  '  if (exit === "process.exit") process.exit(0);',
  '});',
  'process.send("ready");',
].join('\n');

const rawChildScript = `process.stdin.setRawMode(true);\n${childScript}`;

async function checkExit(exit, args, nodeOptions, restore, script = childScript) {
  const initialFlags = stty(['-a']);
  assert.match(initialFlags, /(?:^|[\s;])icanon(?:[\s;]|$)/);
  assert.match(initialFlags, /(?:^|[\s;])echo(?:[\s;]|$)/);

  const child = spawn(process.execPath, [...args, '-e', script], {
    env: { ...process.env, NODE_OPTIONS: nodeOptions },
    stdio: ['inherit', 'pipe', 'inherit', 'ipc'],
  });
  const exited = once(child, 'exit');

  try {
    // The child must capture the startup settings before stty changes them.
    const [message] = await Promise.race([once(child, 'message'), exited]);
    assert.strictEqual(message, 'ready');
    stty(['-echo']);
    const changedFlags = stty(['-a']);
    assert.match(changedFlags, /(?:^|[\s;])-echo(?:[\s;]|$)/);

    if (exit === 'SIGINT') {
      assert.strictEqual(child.kill('SIGINT'), true);
    } else {
      child.send(exit);
    }

    const [code, signal] = await exited;
    assert.strictEqual(code, exit === 'SIGINT' ? null : 0);
    assert.strictEqual(signal, exit === 'SIGINT' ? 'SIGINT' : null);
    const finalFlags = stty(['-a']);
    assert.match(finalFlags, /(?:^|[\s;])icanon(?:[\s;]|$)/, exit);
    assert.strictEqual(/(?:^|[\s;])echo(?:[\s;]|$)/.test(finalFlags), restore, exit);
  } finally {
    try {
      if (child.exitCode === null && child.signalCode === null) {
        child.kill('SIGKILL');
        await exited;
      }
    } finally {
      stty(['echo']);
    }
  }
}

async function main() {
  for (const exit of ['natural', 'process.exit', 'SIGINT']) {
    await checkExit(exit, ['--no-restore-terminal-state'], '', false);
    await checkExit(exit, [], '', true);
    await checkExit(exit, ['--no-restore-terminal-state'], '', true, rawChildScript);
  }
  if (!process.config.variables.node_without_node_options) {
    await checkExit('natural', [], '--no-restore-terminal-state', false);
    await checkExit('natural', ['--restore-terminal-state'],
                    '--no-restore-terminal-state', true);
  }
}

main().then(common.mustCall());
