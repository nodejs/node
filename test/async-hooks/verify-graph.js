'use strict';

const assert = require('assert');
const util = require('util');
require('../common');

function findInGraph(graph, type, n) {
  let found = 0;
  for (let i = 0; i < graph.length; i++) {
    const node = graph[i];
    if (node.type === type) found++;
    if (found === n) return node;
  }
}

function pruneTickObjects(activities) {
  // Remove one TickObject on each pass until none is left anymore
  // not super efficient, but simplest especially to handle
  // multiple TickObjects in a row
  let foundTickObject = true;

  while (foundTickObject) {
    foundTickObject = false;
    let tickObjectIdx = -1;
    for (let i = 0; i < activities.length; i++) {
      if (activities[i].type !== 'TickObject') continue;
      tickObjectIdx = i;
      break;
    }

    if (tickObjectIdx >= 0) {
      foundTickObject = true;

      // Point all triggerAsyncIds that point to the tickObject
      // to its triggerAsyncId and finally remove it from the activities
      const tickObject = activities[tickObjectIdx];
      const newTriggerId = tickObject.triggerAsyncId;
      const oldTriggerId = tickObject.uid;
      activities.forEach(function repointTriggerId(x) {
        if (x.triggerAsyncId === oldTriggerId) x.triggerAsyncId = newTriggerId;
      });
      activities.splice(tickObjectIdx, 1);
    }
  }
  return activities;
}

module.exports = function verifyGraph(hooks, graph) {
  pruneTickObjects(hooks);

  // Map actual ids to standin ids defined in the graph
  const idtouid = {};
  const uidtoid = {};
  const typeSeen = {};
  const errors = [];

  const activities = pruneTickObjects(hooks.activities);
  activities.forEach(processActivity);

  function processActivity(x) {
    if (!typeSeen[x.type]) typeSeen[x.type] = 0;
    typeSeen[x.type]++;

    const node = findInGraph(graph, x.type, typeSeen[x.type]);
    if (node == null) return;

    idtouid[node.id] = x.uid;
    uidtoid[x.uid] = node.id;
    if (node.triggerAsyncId == null) return;

    const tid = idtouid[node.triggerAsyncId];
    if (x.triggerAsyncId === tid) return;

    errors.push({
      id: node.id,
      expectedTid: node.triggerAsyncId,
      actualTid: uidtoid[x.triggerAsyncId]
    });
  }

  if (errors.length) {
    errors.forEach((x) =>
      console.error(
        `'${x.id}' expected to be triggered by '${x.expectedTid}', ` +
        `but was triggered by '${x.actualTid}' instead.`
      )
    );
  }
  assert.strictEqual(errors.length, 0);
};

//
// Helper to generate the input to the verifyGraph tests
//
function inspect(obj, depth) {
  console.error(util.inspect(obj, false, depth || 5, true));
}

module.exports.printGraph = function printGraph(hooks) {
  const ids = {};
  const uidtoid = {};
  const activities = pruneTickObjects(hooks.activities);
  const graph = [];
  activities.forEach(procesNode);

  function procesNode(x) {
    const key = x.type.replace(/WRAP/, '').toLowerCase();
    if (!ids[key]) ids[key] = 1;
    const id = `${key}:${ids[key]++}`;
    uidtoid[x.uid] = id;
    const triggerAsyncId = uidtoid[x.triggerAsyncId] || null;
    graph.push({ type: x.type, id, triggerAsyncId });
  }
  inspect(graph);
};
