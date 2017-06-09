// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SelectionBroker = function() {
  this.brokers = [];
  this.dispatching = false;
  this.lastDispatchingHandler = null;
  this.nodePositionMap = [];
  this.sortedPositionList = [];
  this.positionNodeMap = [];
};

SelectionBroker.prototype.addSelectionHandler = function(handler) {
  this.brokers.push(handler);
}

SelectionBroker.prototype.setNodePositionMap = function(map) {
  let broker = this;
  if (!map) return;
  broker.nodePositionMap = map;
  broker.positionNodeMap = [];
  broker.sortedPositionList = [];
  let next = 0;
  for (let i in broker.nodePositionMap) {
    broker.sortedPositionList[next] = Number(broker.nodePositionMap[i]);
    broker.positionNodeMap[next++] = i;
  }
  broker.sortedPositionList = sortUnique(broker.sortedPositionList,
                                       function(a,b) { return a - b; });
  this.positionNodeMap.sort(function(a,b) {
    let result = broker.nodePositionMap[a] - broker.nodePositionMap[b];
    if (result != 0) return result;
    return a - b;
  });
}

SelectionBroker.prototype.select = function(from, locations, selected) {
  let broker = this;
  if (!broker.dispatching) {
    broker.lastDispatchingHandler = from;
    try {
      broker.dispatching = true;
      let enrichLocations = function(locations) {
        result = [];
        for (let location of locations) {
          let newLocation = {};
          if (location.pos_start != undefined) {
            newLocation.pos_start = location.pos_start;
          }
          if (location.pos_end != undefined) {
            newLocation.pos_end = location.pos_end;
          }
          if (location.node_id != undefined) {
            newLocation.node_id = location.node_id;
          }
          if (location.block_id != undefined) {
            newLocation.block_id = location.block_id;
          }
          if (newLocation.pos_start == undefined &&
              newLocation.pos_end == undefined &&
              newLocation.node_id != undefined) {
            if (broker.nodePositionMap && broker.nodePositionMap[location.node_id]) {
              newLocation.pos_start = broker.nodePositionMap[location.node_id];
              newLocation.pos_end = location.pos_start + 1;
            }
          }
          result.push(newLocation);
        }
        return result;
      }
      locations = enrichLocations(locations);
      for (var b of this.brokers) {
        if (b != from) {
          b.brokeredSelect(locations, selected);
        }
      }
    }
    finally {
      broker.dispatching = false;
    }
  }
}

SelectionBroker.prototype.clear = function(from) {
  this.lastDispatchingHandler = null;
  if (!this.dispatching) {
    try {
      this.dispatching = true;
      this.brokers.forEach(function(b) {
        if (b != from) {
          b.brokeredClear();
        }
      });
    } finally {
      this.dispatching = false;
    }
  }
}
