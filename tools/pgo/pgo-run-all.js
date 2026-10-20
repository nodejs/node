'use strict';

// PGO Training Orchestrator: Runs All PGO Training Workloads
//
// This script orchestrates the execution of all PGO training workloads
// sequentially. It is designed to be run against a PGO-instrumented

// Node.js build to generate profile data (.profraw files on Clang/Clang-CL,
// .gcda files on GCC).
//
// Usage:
//   node tools/pgo/pgo-run-all.js [--duration=<seconds>] [--scripts=<comma-separated>]
//
// Options:
//   --duration=<seconds>   Duration per script in seconds (default: 15)
//   --scripts=<list>       Comma-separated list of scripts to run (default: all)
//                          e.g. --scripts=http-server,json,crypto
//   --sequential           Run scripts one at a time (default, safest for PGO)
//   --verbose              Show detailed output from each script
//
// The scripts are ordered by their importance to real-world Node.js usage:
// 1. HTTP Server      (~60% of Node.js usage is web servers)
// 2. JSON Processing  (every REST API request/response)
// 3. Crypto/TLS       (every HTTPS connection)
// 4. Streams/Buffers  (all I/O goes through these)
// 5. File System      (config loading, static serving, build tools)
// 6. Async Patterns   (Promise/async-await is the concurrency model)
// 7. URL/String       (URL parsing in every request, string ops everywhere)
// 8. Compression      (HTTP response compression)
// 9. Net/DNS          (underlying TCP/DNS for all networking)
// 10. Module Loading  (startup, require() resolution)
// 11. Child/Workers   (build tools, process managers)

const { fork } = require('child_process');
const path = require('path');
const fs = require('fs');

const SCRIPT_DIR = __dirname;

// Training scripts in order of real-world importance
const ALL_SCRIPTS = [
  {
    name: 'http-server',
    file: 'pgo-http-server.js',
    desc: 'HTTP server with JSON APIs, static content, routing',
  },
  {
    name: 'json',
    file: 'pgo-json.js',
    desc: 'JSON parse/stringify with realistic payloads',
  },
  {
    name: 'crypto',
    file: 'pgo-crypto.js',
    desc: 'Hashing, HMAC, AES-GCM, RSA/ECDSA, random, PBKDF2',
  },
  {
    name: 'streams-buffers',
    file: 'pgo-streams-buffers.js',
    desc: 'Buffer creation/encoding, stream pipes, transforms',
  },
  {
    name: 'fs',
    file: 'pgo-fs.js',
    desc: 'File read/write, stat, readdir, streams, path ops',
  },
  {
    name: 'async-patterns',
    file: 'pgo-async-patterns.js',
    desc: 'Promises, EventEmitter, timers, AbortController, ALS',
  },
  {
    name: 'url-string',
    file: 'pgo-url-string.js',
    desc: 'URL parsing, regex, string ops, TextEncoder, util',
  },
  {
    name: 'compression',
    file: 'pgo-compression.js',
    desc: 'Gzip, deflate, brotli compress/decompress',
  },
  {
    name: 'net',
    file: 'pgo-net.js',
    desc: 'TCP server/client, IPC, DNS resolution',
  },
  {
    name: 'module-loading',
    file: 'pgo-module-loading.js',
    desc: 'CJS require, ESM import, VM compilation',
  },
  {
    name: 'child-workers',
    file: 'pgo-child-workers.js',
    desc: 'Worker thread messaging, SharedArrayBuffer, inline eval',
  },
];

function parseArgs() {
  const args = {
    duration: 15,
    scripts: null,
    verbose: false,
  };

  for (const arg of process.argv.slice(2)) {
    if (arg.startsWith('--duration=')) {
      args.duration = parseInt(arg.split('=')[1], 10);
    } else if (arg.startsWith('--scripts=')) {
      args.scripts = arg
        .split('=')[1]
        .split(',')
        .map((s) => s.trim());
    } else if (arg === '--verbose') {
      args.verbose = true;
    } else if (arg === '--help' || arg === '-h') {
      console.log(`
PGO Training Orchestrator for Node.js

Usage: node tools/pgo/pgo-run-all.js [options]

Options:
  --duration=<seconds>   Duration per script (default: 15)
  --scripts=<list>       Comma-separated script names (default: all)
  --verbose              Show script output
  --help                 Show this help

Available scripts:
${ALL_SCRIPTS.map((s) => `  ${s.name.padEnd(20)} ${s.desc}`).join('\n')}

Example:
  node tools/pgo/pgo-run-all.js --duration=20 --verbose
  node tools/pgo/pgo-run-all.js --scripts=http-server,json,crypto --duration=30
`);
      process.exit(0);
    }
  }

  return args;
}

function runScript(scriptPath, duration, verbose) {
  return new Promise((resolve) => {
    const env = {
      ...process.env,
      PGO_TRAINING_DURATION: String(duration * 1000),
    };
    const child = fork(scriptPath, [], {
      stdio: verbose ? 'inherit' : ['ignore', 'pipe', 'pipe', 'ipc'],
      env,
    });

    let output = '';
    if (!verbose && child.stdout) {
      child.stdout.on('data', (data) => {
        output += data.toString();
      });
    }
    if (!verbose && child.stderr) {
      child.stderr.on('data', (data) => {
        output += data.toString();
      });
    }

    // Safety timeout: kill if script runs too long
    const timeout = setTimeout(
      () => {
        console.log('    [TIMEOUT] Killing script...');
        child.kill('SIGTERM');
        setTimeout(() => child.kill('SIGKILL'), 5000);
      },
      (duration + 30) * 1000,
    );

    child.on('exit', (code) => {
      clearTimeout(timeout);
      if (code !== 0 && code !== null) {
        // Extract last line of output for summary
        const lastLine = output.trim().split('\n').pop() || '';
        console.log(`    [WARNING] Exited with code ${code}: ${lastLine}`);
      }
      resolve({ code, output });
    });

    child.on('error', (err) => {
      clearTimeout(timeout);
      console.log(`    [ERROR] ${err.message}`);
      resolve({ code: -1, output: err.message });
    });
  });
}

async function main() {
  const args = parseArgs();

  // Select scripts
  let scripts = ALL_SCRIPTS;
  if (args.scripts) {
    scripts = args.scripts.map((name) => {
      const found = ALL_SCRIPTS.find((s) => s.name === name);
      if (!found) {
        console.error(`Unknown script: ${name}`);
        console.error(
          `Available: ${ALL_SCRIPTS.map((s) => s.name).join(', ')}`,
        );
        process.exit(1);
      }
      return found;
    });
  }

  const totalTime = scripts.length * args.duration;

  console.log(
    '╔══════════════════════════════════════════════════════════════╗',
  );
  console.log('║          Node.js PGO Training Workload Runner              ║');
  console.log(
    '╠══════════════════════════════════════════════════════════════╣',
  );
  console.log(`║  Scripts:  ${String(scripts.length).padEnd(48)}║`);
  console.log(
    `║  Duration: ${String(args.duration + 's per script').padEnd(48)}║`,
  );
  console.log(`║  Total:    ~${String(totalTime + 's estimated').padEnd(47)}║`);
  console.log(`║  Node:     ${process.version.padEnd(48)}║`);
  console.log(
    `║  Arch:     ${(process.arch + ' / ' + process.platform).padEnd(48)}║`,
  );
  console.log(
    '╚══════════════════════════════════════════════════════════════╝',
  );
  console.log('');

  const startTime = Date.now();
  const results = [];

  for (let i = 0; i < scripts.length; i++) {
    const script = scripts[i];
    const scriptPath = path.join(SCRIPT_DIR, script.file);
    const num = `[${i + 1}/${scripts.length}]`;

    console.log(`${num} Running: ${script.name}`);
    console.log(`    ${script.desc}`);

    const scriptStart = Date.now();
    const result = await runScript(scriptPath, args.duration, args.verbose);
    const elapsed = ((Date.now() - scriptStart) / 1000).toFixed(1);

    const status = result.code === 0 ? 'OK' : `FAIL(${result.code})`;
    console.log(`    Completed in ${elapsed}s [${status}]`);

    // Extract ops/s from output if available
    if (!args.verbose && result.output) {
      const match = result.output.match(/(\d+) ops\/s|(\d+) req\/s/);
      if (match) {
        console.log(`    Throughput: ${match[0]}`);
      }
    }
    console.log('');

    results.push({
      name: script.name,
      elapsed: parseFloat(elapsed),
      code: result.code,
    });
  }

  const totalElapsed = ((Date.now() - startTime) / 1000).toFixed(1);
  const passed = results.filter((r) => r.code === 0).length;
  const failed = results.filter((r) => r.code !== 0).length;

  console.log(
    '════════════════════════════════════════════════════════════════',
  );
  console.log(`PGO Training Complete: ${totalElapsed}s total`);
  console.log(
    `  ${passed} passed, ${failed} failed out of ${results.length} scripts`,
  );

  if (failed > 0) {
    console.log(
      `  Failed: ${results
        .filter((r) => r.code !== 0)
        .map((r) => r.name)
        .join(', ')}`,
    );
  }

  // Scan for .profraw files to verify profile data was collected
  const profrawDir = process.env.LLVM_PROFILE_FILE ?
    path.dirname(process.env.LLVM_PROFILE_FILE.replace(/%[mp]/g, '_')) :
    process.cwd();
  let profrawFiles = [];
  try {
    profrawFiles = fs
      .readdirSync(profrawDir)
      .filter((f) => f.endsWith('.profraw'));
  } catch {
    // Directory may not exist if LLVM_PROFILE_FILE points elsewhere
  }

  console.log('');
  if (profrawFiles.length > 0) {
    let totalSize = 0;
    for (const f of profrawFiles) {
      try {
        totalSize += fs.statSync(path.join(profrawDir, f)).size;
      } catch {
        // Ignore stat errors
      }
    }
    const sizeMB = (totalSize / (1024 * 1024)).toFixed(1);
    console.log(
      `Profile data: ${profrawFiles.length} .profraw file(s), ${sizeMB} MB total`,
    );
    console.log(`  Location: ${profrawDir}`);
  } else {
    console.log('WARNING: No .profraw files found in ' + profrawDir);
    console.log(
      '  Ensure LLVM_PROFILE_FILE is set and the binary was built with -fprofile-generate',
    );
  }

  console.log('');
  console.log(
    'Profile data should now be available for PGO-optimized rebuild.',
  );
  console.log(
    '════════════════════════════════════════════════════════════════',
  );

  process.exit(failed > 0 ? 1 : 0);
}

main().catch((err) => {
  console.error('PGO Orchestrator error:', err);
  process.exit(1);
});
