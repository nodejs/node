'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');

// Adding a 'keylog' listener to an agent is wired up by maybeEnableKeylog(),
// which attaches the agent's keylog handler to the sockets the agent already
// owns. `agent.sockets` and `agent.freeSockets` map a name to an *array* of
// sockets, so each bucket has to be walked. Treating the buckets themselves as
// sockets threw a TypeError out of `agent.on('keylog', ...)`, which also meant
// the listener was never registered.

// Two servers so the two sockets get different names, which keeps one parked
// in freeSockets instead of being reused by the second request.
const idleServer = http.createServer((req, res) => res.end('idle'));
const busyServer = http.createServer((req, res) => {
  setTimeout(() => res.end('busy'), common.platformTimeout(200));
});

function countSockets(agent) {
  let free = 0;
  let active = 0;
  for (const bucket of Object.values(agent.freeSockets)) free += bucket.length;
  for (const bucket of Object.values(agent.sockets)) active += bucket.length;
  return { free, active };
}

idleServer.listen(0, common.mustCall(() => {
  busyServer.listen(0, common.mustCall(() => {
    const agent = new http.Agent({ keepAlive: true, maxSockets: 4 });

    // First request finishes, so its socket is released into freeSockets.
    http.get({ port: idleServer.address().port, agent }, common.mustCall((res) => {
      res.resume();
      res.on('end', common.mustCall(() => {
        // Second request is still in flight, so its socket is in sockets.
        const req = http.get({ port: busyServer.address().port, agent },
                             common.mustCall((res2) => {
                               res2.resume();
                               res2.on('end', common.mustCall(() => {
                                 agent.destroy();
                                 idleServer.close();
                                 busyServer.close();
                               }));
                             }));

        req.on('socket', common.mustCall(() => {
          setImmediate(common.mustCall(() => {
            const { free, active } = countSockets(agent);
            assert.strictEqual(free, 1);
            assert.strictEqual(active, 1);

            // Used to throw `TypeError: sockets[i].on is not a function`.
            agent.on('keylog', common.mustNotCall());
            assert.strictEqual(agent.listenerCount('keylog'), 1);

            // Every existing socket, idle or in use, is now listening.
            for (const set of [agent.freeSockets, agent.sockets]) {
              for (const bucket of Object.values(set)) {
                for (const socket of bucket) {
                  assert.strictEqual(socket.listenerCount('keylog'), 1);
                }
              }
            }
          }));
        }));
      }));
    }));
  }));
}));
