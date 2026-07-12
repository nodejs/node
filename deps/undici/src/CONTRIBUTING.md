# Contributing to Undici

* [Guides](#guides)
  * [Update `llhttp`](#update-llhttp)
  * [Lint](#lint)
  * [Test](#test)
  * [Coverage](#coverage)
  * [Releases](#releases)
  * [Update `WPTs`](#update-wpts)
  * [Building for externally shared node builtins](#external-builds)
  *  [Benchmarks](#benchmarks)
  *  [Documentation](#documentation)
  *  [Reproduction](#reproduction)
* [Developer's Certificate of Origin 1.1](#developers-certificate-of-origin)
  * [Moderation Policy](#moderation-policy)

<a id="guides"></a>
## Guides

This is a collection of guides on how to run and update `undici`, and how to run different parts of the project.

<a id="update-llhttp"></a>
### Update `llhttp`

The HTTP parser used by `undici` is a WebAssembly build of [`llhttp`](https://github.com/nodejs/llhttp).

While the project itself provides a way to compile targeting WebAssembly, at the moment we embed the sources
directly and compile the module in `undici`.

The `deps/llhttp/include` folder contains the C header files, while the `deps/llhttp/src` folder contains
the C source files needed to compile the module.

The `lib/llhttp` folder contains the `.js` transpiled assets required to implement a parser.

The following are the steps required to perform an update.

#### Clone the [llhttp](https://github.com/nodejs/llhttp) project

```bash
git clone git@github.com:nodejs/llhttp.git

cd llhttp
```

#### Checkout a `llhttp` release

```bash
git checkout <tag>
```

#### Install the `llhttp` dependencies

```bash
npm i
```

#### Run the wasm build script

> This requires [docker](https://www.docker.com/) installed on your machine.

```bash
npm run build:wasm
```

#### Copy the sources to `undici`

```bash
cp build/wasm/*.js <your-path-to-undici>/lib/llhttp/

cp build/wasm/*.js.map <your-path-to-undici>/lib/llhttp/

cp build/wasm/*.d.ts <your-path-to-undici>/lib/llhttp/

cp src/native/api.c src/native/http.c build/c/llhttp.c <your-path-to-undici>/deps/llhttp/src/

cp src/native/api.h build/llhttp.h <your-path-to-undici>/deps/llhttp/include/
```

#### Build the WebAssembly module in `undici`

> This requires [docker](https://www.docker.com/) installed on your machine.

```bash
cd <your-path-to-undici>

npm run build:wasm
```

#### Commit the contents of lib/llhttp

Create a commit which includes all of the updated files in lib/llhttp.

<a id="update-wpts"></a>
### Update `WPTs`

`undici` runs a subset of the [`web-platform-tests`](https://github.com/web-platform-tests/wpt).

### Steps:

`npm run test:wpt` and `node test/web-platform-tests/wpt-runner.mjs setup` will initialize the WPT submodule automatically when it is missing.

If you want to prepare the checkout explicitly, run:

```bash
git submodule update --init --recursive
```

### Run the tests

Run the tests to ensure that any new failures are marked as such.

Before running the tests for the first time, you must setup the testing environment.
```bash
cd test/web-platform-tests
node wpt-runner.mjs setup
```

To run all tests:

```bash
npm run test:wpt
```

To run a subset of tests:
```bash
cd test/web-platform-tests
node wpt-runner.mjs run [filter] [filterb]
```

To run a single file:
```bash
cd test/web-platform-tests
node wpt-runner.mjs run /path/to/test
```

### Debugging

Verbose logging can be enabled by setting the [`NODE_DEBUG`](https://nodejs.org/api/cli.html#node_debugmodule) flag:

```bash
npx cross-env NODE_DEBUG=UNDICI_WPT node --run test:wpt
```

(`npx cross-env` can be omitted on Linux and Mac)

<a id="lint"></a>
### Lint

```bash
npm run lint
```

<a id="test"></a>
### Test

```bash
npm run test
```

<a id="coverage"></a>
### Coverage

```bash
npm run coverage
```

<a id="releases"></a>
### Issuing Releases

Release is automatic on commit to main which bumps the package.json version field.
Use the "Create release PR" github action to generate a release PR.

<a id="external-builds"></a>
### Building for externally shared node builtins

If you are packaging `undici` for a distro, this might help if you would like to use
an unbundled version instead of bundling one in `libnode.so`.

To enable this, pass `EXTERNAL_PATH=/path/to/global/node_modules/undici` to `build/wasm.js`.
Pass this path with `loader.js` appended to `--shared-builtin-undici/undici-path` in Node.js's `configure.py`.
If building on a non-Alpine Linux distribution, you may need to also set the `WASM_CC`, `WASM_CFLAGS`, `WASM_LDFLAGS` and `WASM_LDLIBS` environment variables before running `build/wasm.js`.
Similarly, you can set the `WASM_OPT` environment variable to utilize your own `wasm-opt` optimizer.

<a id="benchmarks"></a>
### Benchmarks

```bash
cd benchmarks && npm i && npm run bench
```

The benchmarks will be available at `http://localhost:3042`.

<a id="documentation"></a>
### Documentation

```bash
cd docs && npm i && npm run serve
```

The documentation will be available at `http://localhost:3000`.

<a id="reproduction"></a>
### Reproduction

When reporting a bug, a high-quality reproduction helps maintainers diagnose and fix the issue quickly. Follow the guidelines below to create a minimal, standalone reproduction script.

#### What Makes a Good Reproduction

- **Standalone**: The script must be self-contained and run without external dependencies beyond `undici` and Node.js built-in modules.
- **Minimal**: Strip everything unrelated to the bug — no production configs, no frameworks, no unnecessary middleware.
- **Reproducible**: Run the script and confirm the bug occurs before submitting.
- **Specific**: Include the exact `undici` API call that triggers the issue (e.g., `Client`, `Pool`, `Agent`, `fetch`, `request`, `stream`, `pipeline`).
- **Run with Node's test runner**: Use `node --test` (Node 20+) or `node test/your-repro.js`.

#### Standalone Server Pattern

Use `createServer` from `node:http` (or `node:https` for TLS-related bugs) to run a local server inside the same script. This avoids external service dependencies and keeps the reproduction fully self-contained.

```javascript
'use strict'

const { test } = require('node:test')
const { createServer } = require('node:http')
const { once } = require('node:events')
const { Client } = require('undici')

// https://github.com/nodejs/undici/issues/XXXX

test('short description of the bug', { timeout: 60000 }, async (t) => {
  t.plan(1)

  // 1. Start a local HTTP server that reproduces the scenario
  const server = createServer({ joinDuplicateHeaders: true }, async (req, res) => {
    // Simulate the bug-triggering response
    res.statusCode = 200
    res.setHeader('content-length', 100)
    res.end('hello'.repeat(20))
  })
  t.after(() => { server.closeAllConnections?.(); server.close() })

  server.listen(0)
  await once(server, 'listening')

  // 2. Create the undici client pointing to the local server
  const client = new Client(`http://localhost:${server.address().port}`)
  t.after(() => client.close())

  // 3. Perform the request that triggers the bug
  const { body } = await client.request({ path: '/', method: 'GET' })

  let responseBody = ''
  for await (const chunk of body) {
    responseBody += chunk
  }

  t.assert.strictEqual(responseBody, 'hello'.repeat(20))
})
```

Save the script as `test/repro-XXXX.js` and run it:

```bash
node --test test/repro-XXXX.js
```

#### Fetch-Based Reproduction

For bugs involving `fetch`, use the same standalone server pattern:

```javascript
'use strict'

const { test } = require('node:test')
const { createServer } = require('node:http')
const { once } = require('node:events')
const { fetch, Agent } = require('undici')

// https://github.com/nodejs/undici/issues/XXXX

test('short description of the fetch bug', { timeout: 60000 }, async (t) => {
  t.plan(1)

  const server = createServer({ joinDuplicateHeaders: true }, async (req, res) => {
    res.statusCode = 200
    res.setHeader('content-type', 'application/json')
    res.end(JSON.stringify({ ok: true }))
  })
  t.after(() => server.close())

  server.listen(0)
  await once(server, 'listening')

  const agent = new Agent({ keepAliveTimeout: 1 })
  t.after(() => agent.close())

  const response = await fetch(`http://localhost:${server.address().port}/`, {
    dispatcher: agent
  })

  await response.body.cancel()

  t.assert.strictEqual(response.status, 200)
})
```

#### Using Pool or Agent

For bugs involving connection pooling or agent behavior:

```javascript
'use strict'

const { test } = require('node:test')
const { createServer } = require('node:http')
const { once } = require('node:events')
const { Pool } = require('undici')

// https://github.com/nodejs/undici/issues/XXXX

test('Pool bug reproduction', { timeout: 60000 }, async (t) => {
  t.plan(1)

  const server = createServer({ joinDuplicateHeaders: true }, (req, res) => {
    res.end('ok')
  })
  t.after(() => server.close())

  server.listen(0)
  await once(server, 'listening')

  const pool = new Pool(`http://localhost:${server.address().port}`)
  t.after(() => pool.close())

  const data = await pool.request({ path: '/', method: 'GET' })
  await data.body.dump()

  t.assert.strictEqual(data.statusCode, 200)
})
```

#### Adding the Reproduction to the Repository

If you are submitting a bug fix via a pull request, include a test file `test/issue-XXXX.js` that reproduces the bug and passes with the fix.

#### Checklist for Submitting a Bug Report

1. [ ] Write a standalone reproduction script.
2. [ ] Run it locally to confirm the bug is reproducible.
3. [ ] Attach the script (or a gist link) in the bug report.
4. [ ] Include the exact undici version and Node.js version.
5. [ ] Describe the expected vs. actual behavior.

<a id="developers-certificate-of-origin"></a>
## Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

* (a) The contribution was created in whole or in part by me and I
  have the right to submit it under the open source license
  indicated in the file; or

* (b) The contribution is based upon previous work that, to the best
  of my knowledge, is covered under an appropriate open source
  license and I have the right under that license to submit that
  work with modifications, whether created in whole or in part
  by me, under the same open source license (unless I am
  permitted to submit under a different license), as indicated
  in the file; or

* (c) The contribution was provided directly to me by some other
  person who certified (a), (b) or (c) and I have not modified
  it.

* (d) I understand and agree that this project and the contribution
  are public and that a record of the contribution (including all
  personal information I submit with it, including my sign-off) is
  maintained indefinitely and may be redistributed consistent with
  this project or the open source license(s) involved.

<a id="moderation-policy"></a>
### Moderation Policy

The [Node.js Moderation Policy] applies to this project.

[Node.js Moderation Policy]: https://github.com/nodejs/admin/blob/main/Moderation-Policy.md
