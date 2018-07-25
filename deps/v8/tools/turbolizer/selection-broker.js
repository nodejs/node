// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SelectionBroker {
  constructor(sourceResolver) {
    this.sourcePositionHandlers = [];
    this.nodeHandlers = [];
    this.blockHandlers = [];
    this.sourceResolver = sourceResolver;
  };

  addSourcePositionHandler(handler) {
    this.sourcePositionHandlers.push(handler);
  }

  addNodeHandler(handler) {
    this.nodeHandlers.push(handler);
  }

  addBlockHandler(handler) {
    this.blockHandlers.push(handler);
  }

  broadcastSourcePositionSelect(from, sourcePositions, selected) {
    let broker = this;
    sourcePositions = sourcePositions.filter((l) => {
      if (typeof l.scriptOffset == 'undefined'
        || typeof l.inliningId == 'undefined') {
        console.log("Warning: invalid source position");
        return false;
      }
      return true;
    });
    for (var b of this.sourcePositionHandlers) {
      if (b != from) b.brokeredSourcePositionSelect(sourcePositions, selected);
    }
    const nodes = this.sourceResolver.sourcePositionsToNodeIds(sourcePositions);
    for (var b of this.nodeHandlers) {
      if (b != from) b.brokeredNodeSelect(nodes, selected);
    }
  }

  broadcastNodeSelect(from, nodes, selected) {
    let broker = this;
    for (var b of this.nodeHandlers) {
      if (b != from) b.brokeredNodeSelect(nodes, selected);
    }
    const sourcePositions = this.sourceResolver.nodeIdsToSourcePositions(nodes);
    for (var b of this.sourcePositionHandlers) {
      if (b != from) b.brokeredSourcePositionSelect(sourcePositions, selected);
    }
  }

  broadcastBlockSelect(from, blocks, selected) {
    let broker = this;
    for (var b of this.blockHandlers) {
      if (b != from) b.brokeredBlockSelect(blocks, selected);
    }
  }

  broadcastClear(from) {
    this.sourcePositionHandlers.forEach(function (b) {
      if (b != from) b.brokeredClear();
    });
    this.nodeHandlers.forEach(function (b) {
      if (b != from) b.brokeredClear();
    });
    this.blockHandlers.forEach(function (b) {
      if (b != from) b.brokeredClear();
    });
  }
}
