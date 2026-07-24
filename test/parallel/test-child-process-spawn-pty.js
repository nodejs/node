'use strict';

const common = require('../common');
const assert = require('assert');
const { spawn, spawnSync } = require('child_process');

const isTTYScript = [
  'console.log(JSON.stringify({',
  '  stdout: !!process.stdout.isTTY,',
  '  stderr: !!process.stderr.isTTY,',
  '}));',
].join('');

// ConPTY may sprinkle VT sequences and OSC title updates around output.
function stripVt(s) {
  return s
    .replace(/\u001b\[[0-9;?]*[ -/]*[@-~]/g, '')
    .replace(/\u001b\][^\u0007\u001b]*(?:\u0007|\u001b\\)/g, '')
    .trim();
}

function parsePtyJson(stdout) {
  const text = stripVt(stdout.toString());
  try {
    return JSON.parse(text);
  } catch {
    // Fall back to extracting the first JSON object if residual control
    // sequences remain around the payload.
    const match = /\{[\s\S]*\}/.exec(text);
    assert.ok(match, `expected JSON in PTY output: ${JSON.stringify(text)}`);
    return JSON.parse(match[0]);
  }
}

function runWithPty(spawnFn, extra = {}) {
  return spawnFn(process.execPath, ['-e', isTTYScript], {
    pty: { cols: 80, rows: 24 },
    stdio: ['pipe', 'pty', 'pty'],
    ...extra,
  });
}

{
  const result = runWithPty(spawnSync);
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  assert.strictEqual(result.stderr, null);
  const parsed = parsePtyJson(result.stdout);
  assert.strictEqual(parsed.stdout, true);
  assert.strictEqual(parsed.stderr, true);
}

{
  // pty:true must remap pipe/pipe/pipe to the PTY stdio layout.
  const result = spawnSync(process.execPath, ['-e', isTTYScript], {
    pty: true,
    stdio: ['pipe', 'pipe', 'pipe'],
  });
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  assert.strictEqual(result.stderr, null);
  const parsed = parsePtyJson(result.stdout);
  assert.strictEqual(parsed.stdout, true);
  assert.strictEqual(parsed.stderr, true);
}

{
  // stdio: 'pty' implies options.pty.
  const result = spawnSync(process.execPath, ['-e', isTTYScript], {
    stdio: 'pty',
  });
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  assert.strictEqual(result.stderr, null);
  const parsed = parsePtyJson(result.stdout);
  assert.strictEqual(parsed.stdout, true);
  assert.strictEqual(parsed.stderr, true);
}

{
  // Non-empty input must not hang or CTRL_C the PTY child on Windows.
  const result = spawnSync(process.execPath, ['-e', isTTYScript], {
    pty: true,
    input: 'ignored-input',
  });
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  assert.strictEqual(result.stderr, null);
  const parsed = parsePtyJson(result.stdout);
  assert.strictEqual(parsed.stdout, true);
  assert.strictEqual(parsed.stderr, true);
}

{
  // pty.name replaces any existing TERM in the child environment.
  const result = spawnSync(process.execPath, ['-e', 'console.log(process.env.TERM)'], {
    pty: { cols: 80, rows: 24, name: 'xterm-256color' },
    env: { ...process.env, TERM: 'vt100' },
  });
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  assert.strictEqual(stripVt(result.stdout.toString()), 'xterm-256color');
}

{
  // On Windows, env keys are case-insensitive; pty.name must still win.
  if (common.isWindows) {
    const env = { ...process.env };
    for (const key of Object.keys(env)) {
      if (key.toUpperCase() === 'TERM')
        delete env[key];
    }
    env.Term = 'vt100';
    const result = spawnSync(process.execPath, ['-e', 'console.log(process.env.TERM)'], {
      pty: { cols: 80, rows: 24, name: 'xterm-256color' },
      env,
    });
    assert.strictEqual(result.status, 0, `status=${result.status}`);
    assert.strictEqual(stripVt(result.stdout.toString()), 'xterm-256color');
  }
}

{
  // shell + pty: the shell is attached to the PTY.
  // Pass a single command string so shell mode does not concatenate args
  // (avoids DEP0190 and cmd.exe mangling).
  const result = spawnSync(
    `"${process.execPath}" -e "console.log(!!process.stdout.isTTY)"`,
    { pty: true, shell: true },
  );
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  assert.strictEqual(result.stderr, null);
  assert.match(stripVt(result.stdout.toString()), /^true$/m);
}

{
  // pty:false disables PTY mode.
  const result = spawnSync(process.execPath, ['-e', isTTYScript], {
    pty: false,
  });
  assert.strictEqual(result.status, 0, `status=${result.status}`);
  const parsed = parsePtyJson(result.stdout);
  assert.strictEqual(parsed.stdout, false);
  assert.strictEqual(parsed.stderr, false);
}

{
  assert.throws(() => {
    spawnSync(process.execPath, ['-e', '0'], {
      pty: false,
      stdio: 'pty',
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  assert.throws(() => {
    spawnSync(process.execPath, ['-e', '0'], {
      pty: { cols: 0, rows: 24 },
    });
  }, {
    code: 'ERR_OUT_OF_RANGE',
  });
}

{
  assert.throws(() => {
    spawnSync(process.execPath, ['-e', '0'], {
      pty: true,
      windowsHide: true,
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  assert.throws(() => {
    spawnSync(process.execPath, ['-e', '0'], {
      pty: true,
      stdio: 'inherit',
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  assert.throws(() => {
    spawn(process.execPath, ['-e', '0'], {
      pty: true,
      stdio: ['pipe', 'pty', 'pty', 'ipc'],
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  assert.throws(() => {
    spawnSync(process.execPath, ['-e', '0'], {
      pty: true,
      stdio: ['pty', 'pty', 'pty'],
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  const child = runWithPty(spawn);
  let output = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => {
    output += chunk;
  });
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    const parsed = parsePtyJson(output);
    assert.strictEqual(parsed.stdout, true);
    assert.strictEqual(parsed.stderr, true);
    assert.strictEqual(child.stderr, null);
    assert.throws(() => child.resize(80, 24), { code: 'ERR_INVALID_STATE' });
  }));
  child.on('spawn', common.mustCall(() => {
    assert.throws(() => child.resize(0, 24), { code: 'ERR_OUT_OF_RANGE' });
    assert.strictEqual(child.resize(100, 40), 0);
  }));
}

{
  if (common.isWindows) {
    assert.throws(() => {
      spawn(process.execPath, ['-e', '0'], {
        pty: true,
        detached: true,
      });
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
    });
  } else {
    // detached + pty remains supported on non-Windows.
    const child = spawn(process.execPath, ['-e', isTTYScript], {
      pty: true,
      detached: true,
    });
    let output = '';
    child.stdout.setEncoding('utf8');
    child.stdout.on('data', (chunk) => {
      output += chunk;
    });
    child.on('spawn', common.mustCall(() => {
      assert.ok(child.pid > 0);
    }));
    child.on('close', common.mustCall((code) => {
      assert.strictEqual(code, 0);
      const parsed = parsePtyJson(output);
      assert.strictEqual(parsed.stdout, true);
      assert.strictEqual(parsed.stderr, true);
    }));
  }
}

{
  const child = spawn(process.execPath, ['-e', isTTYScript], {
    stdio: 'pipe',
  });
  let output = '';
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (chunk) => {
    output += chunk;
  });
  child.on('spawn', common.mustCall(() => {
    assert.throws(() => child.resize(80, 24), { code: 'ERR_INVALID_STATE' });
  }));
  child.on('close', common.mustCall((code) => {
    assert.strictEqual(code, 0);
    const parsed = parsePtyJson(output);
    assert.strictEqual(parsed.stdout, false);
    assert.strictEqual(parsed.stderr, false);
  }));
}
