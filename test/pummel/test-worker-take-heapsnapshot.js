// Flags: --expose-internals
'use strict';
const common = require('../common');
const { recordState } = require('../common/heap');
const { Worker } = require('worker_threads');
const { once } = require('events');

(async function() {
  const w = new Worker('setInterval(() => {}, 100)', { eval: true });

  await once(w, 'online');
  const stream = await w.getHeapSnapshot();
  const snapshot = recordState(stream);
  snapshot.validateSnapshot('Node / MessagePort', [
    {
      children: [
        { node_name: 'Node / MessagePortData', edge_name: 'data' },
      ],
    },
  ], { loose: true });
  await w.terminate();
})().then(common.mustCall());
