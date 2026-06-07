import { execFile } from 'node:child_process';
import { readFile, readdir, access } from 'node:fs/promises';
import { join } from 'node:path';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);

const MAX_OUTPUT = 32_000;

export function truncate(str, limit = MAX_OUTPUT) {
  if (str.length <= limit) return str;
  return str.slice(0, limit) + `\n...[truncated ${str.length - limit} chars]`;
}

export function inferSubsystemFromPath(file) {
  const parts = file.replace(/\\/g, '/').split('/');
  if (parts[0] === 'lib') {
    const idx = parts[1] === 'internal' ? 2 : 1;
    return parts[idx]?.replace(/\.m?js$/, '') || null;
  }
  if (parts[0] === 'src') {
    return parts[1]?.replace(/^node_/, '').replace(/\.(cc|h)$/, '') || null;
  }
  if (parts[0] === 'test') {
    const name = parts[parts.length - 1]?.replace(/^test-/, '').replace(/\.m?js$/, '');
    return name?.split('-')[0] || null;
  }
  if (parts[0] === 'doc' && parts[1] === 'api') return parts[2]?.replace(/\.md$/, '') || null;
  if (parts[0] === 'tools') return 'tools';
  if (parts[0] === 'benchmark') return 'benchmark';
  if (parts[0] === 'deps') return 'deps';
  return null;
}

export function registerTools(server, root) {
  async function exec(cmd, args, { cwd = root, timeout = 30_000, env } = {}) {
    try {
      const { stdout, stderr } = await execFileAsync(cmd, args, {
        cwd,
        timeout,
        maxBuffer: 10 * 1024 * 1024,
        env: { ...process.env, FORCE_COLOR: '0', ...env },
      });
      const out = (stdout + (stderr ? `\nSTDERR:\n${stderr}` : '')).trim();
      return { ok: true, output: truncate(out) };
    } catch (err) {
      const out = [
        err.stdout,
        err.stderr ? `STDERR:\n${err.stderr}` : '',
        err.killed ? `[killed: timeout or signal]` : `[exit code: ${err.code}]`,
      ].filter(Boolean).join('\n').trim();
      return { ok: false, output: truncate(out) };
    }
  }

  async function getNodeBin() {
    const devBin = join(root, 'node');
    try {
      await access(devBin);
      return devBin;
    } catch {
      return process.execPath;
    }
  }

  // ── Build ───────────────────────────────────────────────────────────────

  server.tool('configure',
              'Run ./configure to set build flags. Required before the first build or when changing flags.',
              {
                type: 'object',
                properties: {
                  debug: {
                    type: 'boolean',
                    description: 'Build in debug mode (passes --debug)',
                    default: false,
                  },
                  extra_flags: {
                    type: 'array',
                    items: { type: 'string' },
                    description: 'Additional flags, e.g. ["--with-intl=full-icu", "--ninja"]',
                    default: [],
                  },
                },
              },
              async ({ debug = false, extra_flags: extraFlags = [] }) => {
                const args = [];
                if (debug) args.push('--debug');
                args.push(...extraFlags);
                const { ok, output } = await exec('./configure', args, { timeout: 120_000 });
                return {
                  content: [{ type: 'text', text: ok ? `Configure succeeded.\n\n${output}` : `Configure failed.\n\n${output}` }],
                  isError: !ok,
                };
              },
  );

  server.tool('build',
              'Build Node.js. Pass target="" for the default release build (equivalent to `make -j4`).',
              {
                type: 'object',
                properties: {
                  target: {
                    type: 'string',
                    description: 'Make target (e.g. "", "test-only", "lint", "doc-only")',
                    default: '',
                  },
                  jobs: {
                    type: 'integer',
                    description: 'Parallel jobs (-j). Default: 4.',
                    default: 4,
                  },
                },
              },
              async ({ target = '', jobs = 4 }) => {
                const args = [`-j${jobs}`];
                if (target) args.push(target);
                const { ok, output } = await exec('make', args, { timeout: 300_000 });
                return {
                  content: [{ type: 'text', text: ok ? `Build succeeded.\n\n${output}` : `Build failed.\n\n${output}` }],
                  isError: !ok,
                };
              },
  );

  // ── Test ────────────────────────────────────────────────────────────────

  server.tool('run_test',
              'Run a single test file with the dev Node.js binary (fast, no test runner overhead).',
              {
                type: 'object',
                properties: {
                  file: {
                    type: 'string',
                    description: 'Test file path relative to repo root, e.g. "test/parallel/test-stream2-transform.js"',
                  },
                  flags: {
                    type: 'array',
                    items: { type: 'string' },
                    description: 'Extra Node.js flags, e.g. ["--expose-internals"]',
                    default: [],
                  },
                },
                required: ['file'],
              },
              async ({ file, flags = [] }) => {
                const nodeBin = await getNodeBin();
                const abs = join(root, file);
                const { ok, output } = await exec(nodeBin, [...flags, abs], { timeout: 60_000 });
                return {
                  content: [{ type: 'text', text: ok ? `Test passed.\n\n${output}` : `Test failed.\n\n${output}` }],
                  isError: !ok,
                };
              },
  );

  server.tool('run_tests',
              'Run tests matching a pattern or subsystem using tools/test.py.',
              {
                type: 'object',
                properties: {
                  pattern: {
                    type: 'string',
                    description: 'Glob pattern or subsystem name, e.g. "parallel/test-stream-*" or "child-process"',
                  },
                  timeout: {
                    type: 'integer',
                    description: 'Per-test timeout in seconds. Default: 60.',
                    default: 60,
                  },
                },
                required: ['pattern'],
              },
              async ({ pattern, timeout: perTestTimeout = 60 }) => {
                const nodeBin = await getNodeBin();
                const args = [join(root, 'tools/test.py'), `--timeout=${perTestTimeout}`, '--no-progress', pattern];
                const { ok, output } = await exec(nodeBin, args, { timeout: 300_000 });
                return {
                  content: [{ type: 'text', text: ok ? `Tests passed.\n\n${output}` : `Tests failed.\n\n${output}` }],
                  isError: !ok,
                };
              },
  );

  // ── Code Search ─────────────────────────────────────────────────────────

  server.tool('search_code',
              'Search for a pattern in the Node.js source (grep -rn). Searches lib/, src/, test/ by default.',
              {
                type: 'object',
                properties: {
                  pattern: {
                    type: 'string',
                    description: 'Search pattern (extended regex)',
                  },
                  dir: {
                    type: 'string',
                    description: 'Directory to search in, relative to repo root. Default: searches lib/, src/, test/.',
                    default: '',
                  },
                  include: {
                    type: 'string',
                    description: 'File glob filter, e.g. "*.js" or "*.cc"',
                    default: '',
                  },
                  ignore_case: {
                    type: 'boolean',
                    description: 'Case-insensitive search',
                    default: false,
                  },
                },
                required: ['pattern'],
              },
              async ({ pattern, dir = '', include = '', ignore_case: ignoreCase = false }) => {
                const args = ['-rn', '--include=*.js', '--include=*.cc', '--include=*.h'];
                if (ignoreCase) args.push('-i');
                if (include) {
                  const idx = args.indexOf('--include=*.js');
                  args.splice(idx, 3, `--include=${include}`);
                }
                args.push('--', pattern);
                const targets = dir ?
                  [join(root, dir)] :
                  [join(root, 'lib'), join(root, 'src'), join(root, 'test')];
                args.push(...targets);
                const { ok, output } = await exec('grep', args, { timeout: 15_000 });
                if (!ok && !output) return { content: [{ type: 'text', text: 'No matches found.' }] };
                return { content: [{ type: 'text', text: truncate(output, 8_000) }] };
              },
  );

  // ── Documentation ───────────────────────────────────────────────────────

  server.tool('list_docs',
              'List available API documentation files in doc/api/.',
              { type: 'object', properties: {} },
              async () => {
                try {
                  const files = await readdir(join(root, 'doc/api'));
                  const sorted = files.filter((f) => f.endsWith('.md')).sort().join('\n');
                  return { content: [{ type: 'text', text: sorted }] };
                } catch (err) {
                  return { content: [{ type: 'text', text: `Error: ${err.message}` }], isError: true };
                }
              },
  );

  server.tool('read_doc',
              'Read an API documentation file from doc/api/.',
              {
                type: 'object',
                properties: {
                  name: {
                    type: 'string',
                    description: 'Doc filename, e.g. "mcp.md" or "stream.md"',
                  },
                },
                required: ['name'],
              },
              async ({ name }) => {
                const filename = name.endsWith('.md') ? name : `${name}.md`;
                try {
                  const content = await readFile(join(root, 'doc/api', filename), 'utf8');
                  return { content: [{ type: 'text', text: truncate(content, 16_000) }] };
                } catch {
                  try {
                    const files = await readdir(join(root, 'doc/api'));
                    const match = files.find((f) => f.toLowerCase() === filename.toLowerCase());
                    if (match) {
                      const content = await readFile(join(root, 'doc/api', match), 'utf8');
                      return { content: [{ type: 'text', text: truncate(content, 16_000) }] };
                    }
                  } catch {
                    // Fall through to not-found return below
                  }
                  return { content: [{ type: 'text', text: `Doc not found: ${filename}` }], isError: true };
                }
              },
  );

  // ── Subsystem / Review ──────────────────────────────────────────────────

  server.tool('find_subsystem',
              'Given changed files, identify the primary Node.js subsystem, reviewers, and PR labels.',
              {
                type: 'object',
                properties: {
                  files: {
                    type: 'array',
                    items: { type: 'string' },
                    description: 'File paths relative to repo root',
                  },
                },
                required: ['files'],
              },
              async ({ files }) => {
                const counts = {};
                const labels = new Set();

                for (const file of files) {
                  const s = inferSubsystemFromPath(file);
                  if (s) counts[s] = (counts[s] || 0) + 1;
                  if (file.startsWith('test/')) labels.add('test');
                  if (file.startsWith('doc/')) labels.add('doc');
                  if (file.startsWith('benchmark/')) labels.add('benchmark');
                }

                for (const file of files.slice(0, 5)) {
                  const { ok, output } = await exec('git', ['log', '--oneline', '-10', '--', file], { timeout: 8_000 });
                  if (!ok || !output) continue;
                  for (const line of output.split('\n')) {
                    const m = line.match(/^[a-f0-9]+ ([a-z][a-z0-9_/-]*(?:,[a-z][a-z0-9_/-]*)*):/);
                    if (m) {
                      for (const s of m[1].split(',').map((x) => x.trim())) {
                        counts[s] = (counts[s] || 0) + 2;
                      }
                    }
                  }
                }

                const sorted = Object.entries(counts).sort((a, b) => b[1] - a[1]).map(([s]) => s);
                const subsystem = sorted[0] || 'unknown';
                labels.add(subsystem);

                return {
                  content: [{
                    type: 'text',
                    text: JSON.stringify({
                      subsystem,
                      likelyReviewers: sorted.slice(0, 3),
                      labels: [...labels].sort(),
                    }, null, 2),
                  }],
                };
              },
  );

  server.tool('list_relevant_tests',
              'Given changed files, suggest which test commands to run.',
              {
                type: 'object',
                properties: {
                  changedFiles: {
                    type: 'array',
                    items: { type: 'string' },
                    description: 'Changed file paths relative to repo root',
                  },
                },
                required: ['changedFiles'],
              },
              async ({ changedFiles }) => {
                const modules = new Set();
                for (const file of changedFiles) {
                  const s = inferSubsystemFromPath(file);
                  if (s && !['tools', 'benchmark', 'deps', 'doc'].includes(s)) modules.add(s);
                }

                const commands = [];
                const reasons = [];

                for (const file of changedFiles) {
                  if (file.startsWith('test/')) commands.push(`./node ${file}`);
                }

                for (const mod of modules) {
                  const checks = [
                    { pattern: `test-${mod}*.js`, cmd: `tools/test.py -J parallel/test-${mod}*` },
                    { pattern: `test-whatwg-${mod}*.js`, cmd: `tools/test.py -J parallel/test-whatwg-${mod}*` },
                  ];
                  for (const { pattern, cmd } of checks) {
                    const { output } = await exec(
                      'find', [join(root, 'test/parallel'), '-name', pattern], { timeout: 10_000 },
                    );
                    if (output?.trim()) { commands.push(cmd); reasons.push(mod); }
                  }
                  const { output: wpt } = await exec('find', [join(root, 'test/wpt'), '-name', '*.js', '-path', `*${mod}*`], { timeout: 10_000 });
                  if (wpt?.trim()) commands.push(`tools/test.py wpt/${mod}`);
                }

                return {
                  content: [{
                    type: 'text',
                    text: JSON.stringify({
                      commands: [...new Set(commands)],
                      reason: reasons.length ? `${[...new Set(reasons)].join(', ')} implementation affected` : 'Related tests found for changed files.',
                    }, null, 2),
                  }],
                };
              },
  );

  server.tool('explain_test_failure',
              'Parse a test failure log (TAP or tools/test.py) and return failures and re-run commands.',
              {
                type: 'object',
                properties: {
                  log: {
                    type: 'string',
                    description: 'Raw test output log',
                  },
                  platform: {
                    type: 'string',
                    enum: ['linux', 'darwin', 'win32'],
                    description: 'Platform the test ran on (optional context)',
                    default: 'linux',
                  },
                },
                required: ['log'],
              },
              async ({ log }) => {
                const failures = [];
                const lines = log.split('\n');

                for (let i = 0; i < lines.length; i++) {
                  const line = lines[i];
                  const tap = line.match(/^not ok \d+ - (.+)/);
                  if (tap) {
                    const diagnostics = [];
                    for (let j = i + 1; j < Math.min(i + 30, lines.length); j++) {
                      if (lines[j].startsWith('#') || lines[j].startsWith('  ')) {
                        diagnostics.push(lines[j].replace(/^#\s*/, '').trim());
                      } else if (/^(ok|not ok)\s/.test(lines[j])) break;
                    }
                    failures.push({ test: tap[1].trim(), details: diagnostics.join('\n').trim() });
                    continue;
                  }
                  const py = line.match(/^(?:FAIL|FAILED)\s+(test\/\S+)/);
                  if (py) failures.push({ test: py[1], details: '' });
                }

                const errorMessages = lines
        .filter((l) => /^\s*([A-Z][a-zA-Z]*Error|assert\.)/.test(l))
        .slice(0, 5)
        .map((l) => l.trim());

                const rerunCommands = [...new Set(
                  failures.map((f) => (f.test.startsWith('test/') ? `./node ${f.test}` : `tools/test.py ${f.test}`)),
                )].slice(0, 10);

                return {
                  content: [{
                    type: 'text',
                    text: failures.length === 0 ?
                      'No test failures detected in the provided log.' :
                      JSON.stringify({
                        failureCount: failures.length,
                        failures: failures.slice(0, 20),
                        errorMessages,
                        rerunCommands,
                      }, null, 2),
                  }],
                };
              },
  );

  server.tool('search_docs',
              'Search Node.js documentation (doc/api, doc/contributing, test/README.md) for a query.',
              {
                type: 'object',
                properties: {
                  query: {
                    type: 'string',
                    description: 'Search pattern (extended regex)',
                  },
                  section: {
                    type: 'string',
                    enum: ['api', 'contributing', 'all'],
                    description: 'Which docs to search. Default: "all"',
                    default: 'all',
                  },
                },
                required: ['query'],
              },
              async ({ query, section = 'all' }) => {
                const targets = [];
                if (section !== 'contributing') targets.push(join(root, 'doc/api'));
                if (section !== 'api') targets.push(join(root, 'doc/contributing'));
                if (section === 'all') targets.push(join(root, 'test/README.md'));

                const args = ['-rn', '-i', '--include=*.md', '--', query, ...targets];
                const { ok, output } = await exec('grep', args, { timeout: 10_000 });
                if (!ok && !output) return { content: [{ type: 'text', text: 'No documentation matches found.' }] };
                return { content: [{ type: 'text', text: truncate(output, 8_000) }] };
              },
  );

  // ── PR Metadata ─────────────────────────────────────────────────────────

  server.tool('get_pr_metadata',
              'Fetch PR metadata: labels, CI status, reviews, and commits for a nodejs/node PR.',
              {
                type: 'object',
                properties: {
                  pr: {
                    type: 'string',
                    description: 'PR number or full GitHub PR URL',
                  },
                  repo: {
                    type: 'string',
                    description: 'GitHub repo in owner/repo format. Default: "nodejs/node"',
                    default: 'nodejs/node',
                  },
                  include_landing_metadata: {
                    type: 'boolean',
                    description: 'Also run `git node metadata` to get the Reviewed-By / PR-URL block',
                    default: false,
                  },
                },
                required: ['pr'],
              },
              async ({ pr, repo = 'nodejs/node', include_landing_metadata: includeLandingMetadata = false }) => {
                const prNum = String(pr).match(/(\d+)\/?$/)?.[1] ?? String(pr);
                const result = { pr: prNum, repo };

                const { ok, output } = await exec('gh', [
                  'pr', 'view', prNum, '--repo', repo,
                  '--json', 'number,title,state,labels,reviews,statusCheckRollup,commits,author,url',
                ], { timeout: 30_000 });

                if (ok) {
                  try {
                    const d = JSON.parse(output);
                    result.title = d.title;
                    result.state = d.state;
                    result.url = d.url;
                    result.author = d.author?.login;
                    result.labels = (d.labels ?? []).map((l) => l.name);

                    const approved = (d.reviews ?? [])
                      .filter((r) => r.state === 'APPROVED')
                      .map((r) => r.author?.login);
                    const changes = (d.reviews ?? [])
                      .filter((r) => r.state === 'CHANGES_REQUESTED')
                      .map((r) => r.author?.login);
                    result.reviews = { approved, changesRequested: changes };

                    const checks = d.statusCheckRollup ?? [];
                    result.ci = {
                      total: checks.length,
                      passed: checks.filter((c) => c.conclusion === 'SUCCESS').length,
                      failed: checks.filter((c) => c.conclusion === 'FAILURE').length,
                      pending: checks.filter((c) => !c.conclusion || c.conclusion === 'PENDING').length,
                    };

                    const commits = d.commits ?? [];
                    result.commitCount = commits.length;
                    if (commits.length > 0) {
                      const last = commits[commits.length - 1];
                      result.latestCommit = { sha: last.oid?.slice(0, 10), message: last.messageHeadline };
                    }
                  } catch (e) {
                    result.error = `gh parse error: ${e.message}`;
                  }
                } else {
                  result.error = truncate(output, 300);
                }

                if (includeLandingMetadata) {
                  const { ok: mOk, output: mOut } = await exec('git', ['node', 'metadata', prNum], { timeout: 20_000 });
                  result.landingMetadata = mOk ? mOut : `git node metadata failed: ${truncate(mOut, 200)}`;
                }

                return { content: [{ type: 'text', text: JSON.stringify(result, null, 2) }] };
              },
  );
}
