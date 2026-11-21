'use strict';

// Range-tree merge for V8 function coverage, based on @bcoe/v8-coverage (MIT):
// https://github.com/demurgos/v8-coverage
//
// V8 omits a nested block when its count equals its parent's. Merging two
// process coverages must recover those omitted uncovered blocks by summing,
// for each source position, the count of the innermost range that contains it
// in each process. See https://github.com/nodejs/node/issues/66860.

const {
  ArrayPrototypePush,
  ArrayPrototypeSort,
  ArrayPrototypeSplice,
  ArrayPrototypeUnshift,
  MathMax,
  SafeMap,
} = primordials;

class RangeTree {
  constructor(start, end, delta, children) {
    this.start = start;
    this.end = end;
    this.delta = delta;
    this.children = children;
  }

  /**
   * @precondition `ranges` are well-formed and pre-order sorted
   */
  static fromSortedRanges(ranges) {
    let root;
    // Stack of parent trees and parent counts.
    const stack = [];
    for (let i = 0; i < ranges.length; ++i) {
      const range = ranges[i];
      const node = new RangeTree(
        range.startOffset, range.endOffset, range.count, [],
      );
      if (root === undefined) {
        root = node;
        ArrayPrototypePush(stack, [node, range.count]);
        continue;
      }
      let parent;
      let parentCount;
      while (true) {
        [parent, parentCount] = stack[stack.length - 1];
        if (range.startOffset < parent.end) {
          break;
        }
        stack.pop();
      }
      node.delta -= parentCount;
      ArrayPrototypePush(parent.children, node);
      ArrayPrototypePush(stack, [node, range.count]);
    }
    return root;
  }

  normalize() {
    const children = [];
    let curEnd;
    let head;
    const tail = [];
    for (let i = 0; i < this.children.length; ++i) {
      const child = this.children[i];
      if (head === undefined) {
        head = child;
      } else if (child.delta === head.delta && child.start === curEnd) {
        ArrayPrototypePush(tail, child);
      } else {
        endChain();
        head = child;
      }
      curEnd = child.end;
    }
    if (head !== undefined) {
      endChain();
    }

    if (children.length === 1) {
      const child = children[0];
      if (child.start === this.start && child.end === this.end) {
        this.delta += child.delta;
        this.children = child.children;
        return;
      }
    }
    this.children = children;

    function endChain() {
      if (tail.length !== 0) {
        head.end = tail[tail.length - 1].end;
        for (let i = 0; i < tail.length; ++i) {
          const tailTree = tail[i];
          for (let j = 0; j < tailTree.children.length; ++j) {
            const subChild = tailTree.children[j];
            subChild.delta += tailTree.delta - head.delta;
            ArrayPrototypePush(head.children, subChild);
          }
        }
        tail.length = 0;
      }
      head.normalize();
      ArrayPrototypePush(children, head);
    }
  }

  /**
   * @precondition `tree.start < value && value < tree.end`
   * @return {RangeTree} Right part
   */
  split(value) {
    let leftChildLen = this.children.length;
    let mid;
    for (let i = 0; i < this.children.length; ++i) {
      const child = this.children[i];
      if (child.start < value && value < child.end) {
        mid = child.split(value);
        leftChildLen = i + 1;
        break;
      } else if (child.start >= value) {
        leftChildLen = i;
        break;
      }
    }
    const rightLen = this.children.length - leftChildLen;
    const rightChildren =
      ArrayPrototypeSplice(this.children, leftChildLen, rightLen);
    if (mid !== undefined) {
      ArrayPrototypeUnshift(rightChildren, mid);
    }
    const result = new RangeTree(value, this.end, this.delta, rightChildren);
    this.end = value;
    return result;
  }

  /**
   * Get the range coverages corresponding to the tree.
   * The ranges are pre-order sorted.
   */
  toRanges() {
    const ranges = [];
    const stack = [[this, 0]];
    while (stack.length > 0) {
      const [cur, parentCount] = stack.pop();
      const count = parentCount + cur.delta;
      ArrayPrototypePush(ranges, {
        __proto__: null,
        startOffset: cur.start,
        endOffset: cur.end,
        count,
      });
      for (let i = cur.children.length - 1; i >= 0; --i) {
        ArrayPrototypePush(stack, [cur.children[i], count]);
      }
    }
    return ranges;
  }
}

class RangeTreeWithParent {
  constructor(parentIndex, tree) {
    this.parentIndex = parentIndex;
    this.tree = tree;
  }
}

class StartEvent {
  constructor(offset, trees) {
    this.offset = offset;
    this.trees = trees;
  }

  static compare(a, b) {
    return a.offset - b.offset;
  }
}

class StartEventQueue {
  constructor(queue) {
    this.queue = queue;
    this.nextIndex = 0;
    this.pendingOffset = 0;
    this.pendingTrees = undefined;
  }

  static fromParentTrees(parentTrees) {
    const startToTrees = new SafeMap();
    for (let parentIndex = 0; parentIndex < parentTrees.length; ++parentIndex) {
      const parentTree = parentTrees[parentIndex];
      for (let i = 0; i < parentTree.children.length; ++i) {
        const child = parentTree.children[i];
        let trees = startToTrees.get(child.start);
        if (trees === undefined) {
          trees = [];
          startToTrees.set(child.start, trees);
        }
        ArrayPrototypePush(
          trees, new RangeTreeWithParent(parentIndex, child),
        );
      }
    }
    const queue = [];
    for (const { 0: startOffset, 1: trees } of startToTrees) {
      ArrayPrototypePush(queue, new StartEvent(startOffset, trees));
    }
    ArrayPrototypeSort(queue, StartEvent.compare);
    return new StartEventQueue(queue);
  }

  setPendingOffset(offset) {
    this.pendingOffset = offset;
  }

  pushPendingTree(tree) {
    if (this.pendingTrees === undefined) {
      this.pendingTrees = [];
    }
    ArrayPrototypePush(this.pendingTrees, tree);
  }

  next() {
    const pendingTrees = this.pendingTrees;
    const nextEvent = this.queue[this.nextIndex];
    if (pendingTrees === undefined) {
      this.nextIndex++;
      return nextEvent;
    } else if (nextEvent === undefined) {
      this.pendingTrees = undefined;
      return new StartEvent(this.pendingOffset, pendingTrees);
    } else if (this.pendingOffset < nextEvent.offset) {
      this.pendingTrees = undefined;
      return new StartEvent(this.pendingOffset, pendingTrees);
    }
    if (this.pendingOffset === nextEvent.offset) {
      this.pendingTrees = undefined;
      for (let i = 0; i < pendingTrees.length; ++i) {
        ArrayPrototypePush(nextEvent.trees, pendingTrees[i]);
      }
    }
    this.nextIndex++;
    return nextEvent;
  }
}

/**
 * @precondition Same `start` and `end` for all the trees
 */
function mergeRangeTrees(trees) {
  if (trees.length <= 1) {
    return trees[0];
  }
  const first = trees[0];
  let delta = 0;
  for (let i = 0; i < trees.length; ++i) {
    delta += trees[i].delta;
  }
  const children = mergeRangeTreeChildren(trees);
  return new RangeTree(first.start, first.end, delta, children);
}

function mergeRangeTreeChildren(parentTrees) {
  const result = [];
  const startEventQueue = StartEventQueue.fromParentTrees(parentTrees);
  const parentToNested = new SafeMap();
  let openRange;
  while (true) {
    const event = startEventQueue.next();
    if (event === undefined) {
      break;
    }

    if (openRange !== undefined && openRange.end <= event.offset) {
      ArrayPrototypePush(result, nextChild(openRange, parentToNested));
      openRange = undefined;
    }

    if (openRange === undefined) {
      let openRangeEnd = event.offset + 1;
      for (let i = 0; i < event.trees.length; ++i) {
        const { parentIndex, tree } = event.trees[i];
        openRangeEnd = MathMax(openRangeEnd, tree.end);
        insertChild(parentToNested, parentIndex, tree);
      }
      startEventQueue.setPendingOffset(openRangeEnd);
      openRange = { __proto__: null, start: event.offset, end: openRangeEnd };
    } else {
      for (let i = 0; i < event.trees.length; ++i) {
        const { parentIndex, tree } = event.trees[i];
        if (tree.end > openRange.end) {
          const right = tree.split(openRange.end);
          startEventQueue.pushPendingTree(
            new RangeTreeWithParent(parentIndex, right),
          );
        }
        insertChild(parentToNested, parentIndex, tree);
      }
    }
  }
  if (openRange !== undefined) {
    ArrayPrototypePush(result, nextChild(openRange, parentToNested));
  }
  return result;
}

function insertChild(parentToNested, parentIndex, tree) {
  let nested = parentToNested.get(parentIndex);
  if (nested === undefined) {
    nested = [];
    parentToNested.set(parentIndex, nested);
  }
  ArrayPrototypePush(nested, tree);
}

function nextChild(openRange, parentToNested) {
  const matchingTrees = [];
  for (const nested of parentToNested.values()) {
    if (nested.length === 1 &&
        nested[0].start === openRange.start &&
        nested[0].end === openRange.end) {
      ArrayPrototypePush(matchingTrees, nested[0]);
    } else {
      ArrayPrototypePush(
        matchingTrees,
        new RangeTree(openRange.start, openRange.end, 0, nested),
      );
    }
  }
  parentToNested.clear();
  return mergeRangeTrees(matchingTrees);
}

function mergeFunctionRanges(oldRanges, newRanges) {
  const mergedTree = mergeRangeTrees([
    RangeTree.fromSortedRanges(oldRanges),
    RangeTree.fromSortedRanges(newRanges),
  ]);
  mergedTree.normalize();
  return mergedTree.toRanges();
}

module.exports = { mergeFunctionRanges };
