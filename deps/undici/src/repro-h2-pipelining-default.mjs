// Repro for the H2 default-pipelining bottleneck described in #4143.
//
// Since 8.0.0 allowH2 defaults to true, but pipelining still defaults to 1.
// On a single shared H2 session (connections=1) that serializes concurrent
// fetch() calls into one in-flight stream at a time, instead of multiplexing.
// Without connections set, Agent works around it by opening one TCP socket
// per concurrent request — defeating H2 multiplexing entirely and creating
// extra TLS handshakes.
//
// Run:
//   node repro-h2-pipelining-default.mjs
//
// Expected output on main:
//   default (allowH2=true, p=1)   total~=1s   sockets=5  h2sessions=5  (one socket per req)
//   connections=1, p=1 (default)  total~=5s   sockets=1  h2sessions=1  (serialized!)
//   connections=1, pipelining=100 total~=1s   sockets=1  h2sessions=1  (multiplexed)

import { createSecureServer } from 'node:http2'
import { once } from 'node:events'
import pem from '@metcoder95/https-pem'
import { fetch, Agent } from './index.js'

const N = 5
const DELAY = 1000

const server = createSecureServer({
  ...(await pem.generate({ opts: { keySize: 2048 } })),
  allowHTTP1: true
})
let inFlight = 0
let peakInFlight = 0
const arrivedAt = []
const sockets = new Set()
const sessions = new Set()
server.on('session', (s) => sessions.add(s))
server.on('connection', (sock) => sockets.add(sock))
server.on('stream', (stream) => {
  arrivedAt.push(Date.now())
  inFlight++
  peakInFlight = Math.max(peakInFlight, inFlight)
  setTimeout(() => {
    inFlight--
    stream.respond({ ':status': 200 })
    stream.end('ok')
  }, DELAY)
})
server.listen(0)
await once(server, 'listening')
const url = `https://localhost:${server.address().port}/`

async function run (label, dispatcher) {
  arrivedAt.length = 0
  peakInFlight = 0
  sockets.clear()
  sessions.clear()
  const t0 = Date.now()
  await Promise.all(
    Array.from({ length: N }, () =>
      fetch(url, { dispatcher }).then(r => r.text())
    )
  )
  const total = Date.now() - t0
  const spreadMs = arrivedAt.at(-1) - arrivedAt[0]
  console.log(
    `${label.padEnd(28)} total=${total}ms  ` +
    `peak=${peakInFlight}  ` +
    `sockets=${sockets.size}  h2sessions=${sessions.size}  ` +
    `spread=${spreadMs}ms`
  )
  await dispatcher.close()
}

const tlsOpts = { connect: { rejectUnauthorized: false } }
await run('default (allowH2=true, p=1)', new Agent(tlsOpts))
await run('connections=1, p=1 (default)', new Agent({ ...tlsOpts, connections: 1 }))
await run('connections=1, pipelining=100', new Agent({ ...tlsOpts, connections: 1, pipelining: 100 }))

server.close()
