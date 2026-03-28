import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';
import path from 'node:path';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import { createInterface } from 'node:readline';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

tmpdir.refresh();

describe('watch mode features', { concurrency: false, timeout: 60_000 }, () => {
  it('should restart manually when "rs" is typed in stdin', async () => {
    const file = path.join(tmpdir.path, 'test-rs.js');
    writeFileSync(file, 'console.log("running");');
    
    const child = spawn(execPath, ['--watch', file], {
      encoding: 'utf8',
      stdio: ['pipe', 'pipe', 'pipe']
    });

    const stdoutLines = [];
    const rl = createInterface({ input: child.stdout });

    let rsTriggered = false;
    
    try {
      for await (const line of rl) {
        stdoutLines.push(line);
        if (line.includes('Completed running')) {
          if (!rsTriggered) {
            child.stdin.write('rs\n');
            rsTriggered = true;
          } else {
            break;
          }
        }
      }
    } finally {
      child.kill();
    }

    const output = stdoutLines.join('\n');
    assert.match(output, /\[node:watch\] Restarting .* due to manual restart/);
    assert.match(output, /running/);
  });

  it('should show the file that triggered the restart', async () => {
    const file = path.join(tmpdir.path, 'test-trigger.js');
    writeFileSync(file, 'console.log("running");');
    
    const child = spawn(execPath, ['--watch', file], {
      encoding: 'utf8',
      stdio: ['pipe', 'pipe', 'pipe']
    });

    const stdoutLines = [];
    const rl = createInterface({ input: child.stdout });

    let fileChanged = false;
    
    try {
      for await (const line of rl) {
        stdoutLines.push(line);
        if (line.includes('Completed running')) {
          if (!fileChanged) {
            writeFileSync(file, 'console.log("running again");');
            fileChanged = true;
          } else {
            break;
          }
        }
      }
    } finally {
      child.kill();
    }

    const output = stdoutLines.join('\n');
    assert.match(output, /\[node:watch\] Restarting .* due to change in:/);
    assert.match(output, new RegExp(path.basename(file)));
  });
});
