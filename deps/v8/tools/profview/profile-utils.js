// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

let codeKinds = [
    "UNKNOWN",
    "CPPPARSE",
    "CPPCOMPBC",
    "CPPCOMP",
    "CPPGC",
    "CPPEXT",
    "CPP",
    "LIB",
    "IC",
    "BC",
    "STUB",
    "BUILTIN",
    "REGEXP",
    "JSOPT",
    "JSUNOPT"
];

function resolveCodeKind(code) {
  if (!code || !code.type) {
    return "UNKNOWN";
  } else if (code.type === "CPP") {
    return "CPP";
  } else if (code.type === "SHARED_LIB") {
    return "LIB";
  } else if (code.type === "CODE") {
    if (code.kind === "LoadIC" ||
        code.kind === "StoreIC" ||
        code.kind === "KeyedStoreIC" ||
        code.kind === "KeyedLoadIC" ||
        code.kind === "LoadGlobalIC" ||
        code.kind === "Handler") {
      return "IC";
    } else if (code.kind === "BytecodeHandler") {
      return "BC";
    } else if (code.kind === "Stub") {
      return "STUB";
    } else if (code.kind === "Builtin") {
      return "BUILTIN";
    } else if (code.kind === "RegExp") {
      return "REGEXP";
    }
    console.log("Unknown CODE: '" + code.kind + "'.");
    return "CODE";
  } else if (code.type === "JS") {
    if (code.kind === "Builtin") {
      return "JSUNOPT";
    } else if (code.kind === "Opt") {
      return "JSOPT";
    } else if (code.kind === "Unopt") {
      return "JSUNOPT";
    }
  }
  console.log("Unknown code type '" + type + "'.");
}

function resolveCodeKindAndVmState(code, vmState) {
  let kind = resolveCodeKind(code);
  if (kind === "CPP") {
    if (vmState === 1) {
      kind = "CPPGC";
    } else if (vmState === 2) {
      kind = "CPPPARSE";
    } else if (vmState === 3) {
      kind = "CPPCOMPBC";
    } else if (vmState === 4) {
      kind = "CPPCOMP";
    } else if (vmState === 6) {
      kind = "CPPEXT";
    }
  }
  return kind;
}

function codeEquals(code1, code2, allowDifferentKinds = false) {
  if (!code1 || !code2) return false;
  if (code1.name !== code2.name || code1.type !== code2.type) return false;

  if (code1.type === 'CODE') {
    if (!allowDifferentKinds && code1.kind !== code2.kind) return false;
  } else if (code1.type === 'JS') {
    if (!allowDifferentKinds && code1.kind !== code2.kind) return false;
    if (code1.func !== code2.func) return false;
  }
  return true;
}

function createNodeFromStackEntry(code, codeId, vmState) {
  let name = code ? code.name : "UNKNOWN";

  return { name, codeId, type : resolveCodeKindAndVmState(code, vmState),
           children : [], ownTicks : 0, ticks : 0 };
}

function childIdFromCode(codeId, code) {
  // For JavaScript function, pretend there is one instance of optimized
  // function and one instance of unoptimized function per SFI.
  // Otherwise, just compute the id from code id.
  let type = resolveCodeKind(code);
  if (type === "JSOPT") {
    return code.func * 4 + 1;
  } else if (type === "JSUNOPT") {
    return code.func * 4 + 2;
  } else {
    return codeId * 4;
  }
}

// We store list of ticks and positions within the ticks stack by
// storing flattened triplets of { tickIndex, depth, count }.
// Triplet { 123, 2, 3 } encodes positions in ticks 123, 124, 125,
// all of them at depth 2. The flattened array is used to encode
// position within the call-tree.

// The following function helps to encode such triplets.
function addFrameToFrameList(paths, pathIndex, depth) {
  // Try to combine with the previous code run.
  if (paths.length > 0 &&
      paths[paths.length - 3] + 1 === pathIndex &&
      paths[paths.length - 2] === depth) {
    paths[paths.length - 1]++;
  } else {
    paths.push(pathIndex, depth, 1);
  }
}

function findNextFrame(file, stack, stackPos, step, filter) {
  let codeId = -1;
  let code = null;
  while (stackPos >= 0 && stackPos < stack.length) {
    codeId = stack[stackPos];
    code = codeId >= 0 ? file.code[codeId] : undefined;

    if (filter) {
      let type = code ? code.type : undefined;
      let kind = code ? code.kind : undefined;
      if (filter(type, kind)) return stackPos;
    }
    stackPos += step;
  }
  return -1;
}

function addOrUpdateChildNode(parent, file, stackIndex, stackPos, ascending) {
  let stack = file.ticks[stackIndex].s;
  let vmState = file.ticks[stackIndex].vm;
  let codeId = stack[stackPos];
  let code = codeId >= 0 ? file.code[codeId] : undefined;
  if (stackPos === -1) {
    // We reached the end without finding the next step.
    // If we are doing top-down call tree, update own ticks.
    if (!ascending) {
      parent.ownTicks++;
    }
  } else {
    console.assert(stackPos >= 0 && stackPos < stack.length);
    // We found a child node.
    let childId = childIdFromCode(codeId, code);
    let child = parent.children[childId];
    if (!child) {
      child = createNodeFromStackEntry(code, codeId, vmState);
      child.delayedExpansion = { frameList : [], ascending };
      parent.children[childId] = child;
    }
    child.ticks++;
    addFrameToFrameList(child.delayedExpansion.frameList, stackIndex, stackPos);
  }
}

// This expands a tree node (direct children only).
function expandTreeNode(file, node, filter) {
  let { frameList, ascending } = node.delayedExpansion;

  let step = ascending ? 2 : -2;

  for (let i = 0; i < frameList.length; i+= 3) {
    let firstStackIndex = frameList[i];
    let depth = frameList[i + 1];
    let count = frameList[i + 2];
    for (let j = 0; j < count; j++) {
      let stackIndex = firstStackIndex + j;
      let stack = file.ticks[stackIndex].s;

      // Get to the next frame that has not been filtered out.
      let stackPos = findNextFrame(file, stack, depth + step, step, filter);
      addOrUpdateChildNode(node, file, stackIndex, stackPos, ascending);
    }
  }
  node.delayedExpansion = null;
}

function createEmptyNode(name) {
  return {
      name : name,
      codeId: -1,
      type : "CAT",
      children : [],
      ownTicks : 0,
      ticks : 0
  };
}

class RuntimeCallTreeProcessor {
  constructor() {
    this.tree = createEmptyNode("root");
    this.tree.delayedExpansion = { frameList : [], ascending : false };
  }

  addStack(file, tickIndex) {
    this.tree.ticks++;

    let stack = file.ticks[tickIndex].s;
    let i;
    for (i = 0; i < stack.length; i += 2) {
      let codeId = stack[i];
      if (codeId < 0) return;
      let code = file.code[codeId];
      if (code.type !== "CPP" && code.type !== "SHARED_LIB") {
        i -= 2;
        break;
      }
    }
    if (i < 0 || i >= stack.length) return;
    addOrUpdateChildNode(this.tree, file, tickIndex, i, false);
  }
}

class PlainCallTreeProcessor {
  constructor(filter, isBottomUp) {
    this.filter = filter;
    this.tree = createEmptyNode("root");
    this.tree.delayedExpansion = { frameList : [], ascending : isBottomUp };
    this.isBottomUp = isBottomUp;
  }

  addStack(file, tickIndex) {
    let stack = file.ticks[tickIndex].s;
    let step = this.isBottomUp ? 2 : -2;
    let start = this.isBottomUp ? 0 : stack.length - 2;

    let stackPos = findNextFrame(file, stack, start, step, this.filter);
    addOrUpdateChildNode(this.tree, file, tickIndex, stackPos, this.isBottomUp);

    this.tree.ticks++;
  }
}

function buildCategoryTreeAndLookup() {
  let root = createEmptyNode("root");
  let categories = {};
  function addCategory(name, types) {
    let n = createEmptyNode(name);
    for (let i = 0; i < types.length; i++) {
      categories[types[i]] = n;
    }
    root.children.push(n);
  }
  addCategory("JS Optimized", [ "JSOPT" ]);
  addCategory("JS Unoptimized", [ "JSUNOPT", "BC" ]);
  addCategory("IC", [ "IC" ]);
  addCategory("RegExp", [ "REGEXP" ]);
  addCategory("Other generated", [ "STUB", "BUILTIN" ]);
  addCategory("C++", [ "CPP", "LIB" ]);
  addCategory("C++/GC", [ "CPPGC" ]);
  addCategory("C++/Parser", [ "CPPPARSE" ]);
  addCategory("C++/Bytecode compiler", [ "CPPCOMPBC" ]);
  addCategory("C++/Compiler", [ "CPPCOMP" ]);
  addCategory("C++/External", [ "CPPEXT" ]);
  addCategory("Unknown", [ "UNKNOWN" ]);

  return { categories, root };
}

class CategorizedCallTreeProcessor {
  constructor(filter, isBottomUp) {
    this.filter = filter;
    let { categories, root } = buildCategoryTreeAndLookup();

    this.tree = root;
    this.categories = categories;
    this.isBottomUp = isBottomUp;
  }

  addStack(file, tickIndex) {
    let stack = file.ticks[tickIndex].s;
    let vmState = file.ticks[tickIndex].vm;
    if (stack.length === 0) return;
    let codeId = stack[0];
    let code = codeId >= 0 ? file.code[codeId] : undefined;
    let kind = resolveCodeKindAndVmState(code, vmState);
    let node = this.categories[kind];

    this.tree.ticks++;
    node.ticks++;

    let step = this.isBottomUp ? 2 : -2;
    let start = this.isBottomUp ? 0 : stack.length - 2;

    let stackPos = findNextFrame(file, stack, start, step, this.filter);
    addOrUpdateChildNode(node, file, tickIndex, stackPos, this.isBottomUp);
  }
}

class FunctionListTree {
  constructor(filter, withCategories) {
    if (withCategories) {
      let { categories, root } = buildCategoryTreeAndLookup();
      this.tree = root;
      this.categories = categories;
    } else {
      this.tree = {
        name : "root",
        codeId: -1,
        children : [],
        ownTicks : 0,
        ticks : 0
      };
      this.categories = null;
    }

    this.codeVisited = [];
    this.filter = filter;
  }

  addStack(file, tickIndex) {
    let stack = file.ticks[tickIndex].s;
    let vmState = file.ticks[tickIndex].vm;

    this.tree.ticks++;
    let child = null;
    let tree = null;
    for (let i = stack.length - 2; i >= 0; i -= 2) {
      let codeId = stack[i];
      if (codeId < 0 || this.codeVisited[codeId]) continue;

      let code = codeId >= 0 ? file.code[codeId] : undefined;
      if (this.filter) {
        let type = code ? code.type : undefined;
        let kind = code ? code.kind : undefined;
        if (!this.filter(type, kind)) continue;
      }
      let childId = childIdFromCode(codeId, code);
      if (this.categories) {
        let kind = resolveCodeKindAndVmState(code, vmState);
        tree = this.categories[kind];
      } else {
        tree = this.tree;
      }
      child = tree.children[childId];
      if (!child) {
        child = createNodeFromStackEntry(code, codeId, vmState);
        child.children[0] = createEmptyNode("Top-down tree");
        child.children[0].delayedExpansion =
          { frameList : [], ascending : false };
        child.children[1] = createEmptyNode("Bottom-up tree");
        child.children[1].delayedExpansion =
          { frameList : [], ascending : true };
        tree.children[childId] = child;
      }
      child.ticks++;
      child.children[0].ticks++;
      addFrameToFrameList(
          child.children[0].delayedExpansion.frameList, tickIndex, i);
      child.children[1].ticks++;
      addFrameToFrameList(
          child.children[1].delayedExpansion.frameList, tickIndex, i);
      this.codeVisited[codeId] = true;
    }
    if (child) {
      child.ownTicks++;
      console.assert(tree !== null);
      tree.ticks++;
      console.assert(tree.type === "CAT");
    }

    for (let i = 0; i < stack.length; i += 2) {
      let codeId = stack[i];
      if (codeId >= 0) this.codeVisited[codeId] = false;
    }
  }
}


class CategorySampler {
  constructor(file, bucketCount) {
    this.bucketCount = bucketCount;

    this.firstTime = file.ticks[0].tm;
    let lastTime = file.ticks[file.ticks.length - 1].tm;
    this.step = (lastTime - this.firstTime) / bucketCount;

    this.buckets = [];
    let bucket = {};
    for (let i = 0; i < codeKinds.length; i++) {
      bucket[codeKinds[i]] = 0;
    }
    for (let i = 0; i < bucketCount; i++) {
      this.buckets.push(Object.assign({ total : 0 }, bucket));
    }
  }

  addStack(file, tickIndex) {
    let { tm : timestamp, vm : vmState, s : stack } = file.ticks[tickIndex];

    let i = Math.floor((timestamp - this.firstTime) / this.step);
    if (i === this.buckets.length) i--;
    console.assert(i >= 0 && i < this.buckets.length);

    let bucket = this.buckets[i];
    bucket.total++;

    let codeId = (stack.length > 0) ? stack[0] : -1;
    let code = codeId >= 0 ? file.code[codeId] : undefined;
    let kind = resolveCodeKindAndVmState(code, vmState);
    bucket[kind]++;
  }
}

class FunctionTimelineProcessor {
  constructor(functionCodeId, filter) {
    this.functionCodeId = functionCodeId;
    this.filter = filter;
    this.blocks = [];
    this.currentBlock = null;
  }

  addStack(file, tickIndex) {
    if (!this.functionCodeId) return;

    let { tm : timestamp, vm : vmState, s : stack } = file.ticks[tickIndex];
    let functionCode = file.code[this.functionCodeId];

    // Find if the function is on the stack, and its position on the stack,
    // ignoring any filtered entries.
    let stackCode = undefined;
    let functionPosInStack = -1;
    let filteredI = 0;
    for (let i = 0; i < stack.length - 1; i += 2) {
      let codeId = stack[i];
      let code = codeId >= 0 ? file.code[codeId] : undefined;
      let type = code ? code.type : undefined;
      let kind = code ? code.kind : undefined;
      if (!this.filter(type, kind)) continue;

      // Match other instances of the same function (e.g. unoptimised, various
      // different optimised versions).
      if (codeEquals(code, functionCode, true)) {
        functionPosInStack = filteredI;
        stackCode = code;
        break;
      }
      filteredI++;
    }

    if (functionPosInStack >= 0) {
      let stackKind = resolveCodeKindAndVmState(stackCode, vmState);

      let codeIsTopOfStack = (functionPosInStack === 0);

      if (this.currentBlock !== null) {
        this.currentBlock.end = timestamp;

        if (codeIsTopOfStack === this.currentBlock.topOfStack
          && stackKind === this.currentBlock.kind) {
          // If we haven't changed the stack top or the function kind, then
          // we're happy just extending the current block and not starting
          // a new one.
          return;
        }
      }

      // Start a new block at the current timestamp.
      this.currentBlock = {
        start: timestamp,
        end: timestamp,
        code: stackCode,
        kind: stackKind,
        topOfStack: codeIsTopOfStack
      };
      this.blocks.push(this.currentBlock);
    } else {
      this.currentBlock = null;
    }
  }
}

// Generates a tree out of a ticks sequence.
// {file} is the JSON files with the ticks and code objects.
// {startTime}, {endTime} is the interval.
// {tree} is the processor of stacks.
function generateTree(
    file, startTime, endTime, tree) {
  let ticks = file.ticks;
  let i = 0;
  while (i < ticks.length && ticks[i].tm < startTime) {
    i++;
  }

  let tickCount = 0;
  while (i < ticks.length && ticks[i].tm < endTime) {
    tree.addStack(file, i);
    i++;
    tickCount++;
  }

  return tickCount;
}

function computeOptimizationStats(file,
    timeStart = -Infinity, timeEnd = Infinity) {
  function newCollection() {
    return { count : 0, functions : [], functionTable : [] };
  }
  function addToCollection(collection, code) {
    collection.count++;
    let funcData = collection.functionTable[code.func];
    if (!funcData) {
      funcData = { f : file.functions[code.func], instances : [] };
      collection.functionTable[code.func] = funcData;
      collection.functions.push(funcData);
    }
    funcData.instances.push(code);
  }

  let functionCount = 0;
  let optimizedFunctionCount = 0;
  let deoptimizedFunctionCount = 0;
  let optimizations = newCollection();
  let eagerDeoptimizations = newCollection();
  let softDeoptimizations = newCollection();
  let lazyDeoptimizations = newCollection();

  for (let i = 0; i < file.functions.length; i++) {
    let f = file.functions[i];

    // Skip special SFIs that do not correspond to JS functions.
    if (f.codes.length === 0) continue;
    if (file.code[f.codes[0]].type !== "JS") continue;

    functionCount++;
    let optimized = false;
    let deoptimized = false;

    for (let j = 0; j < f.codes.length; j++) {
      let code = file.code[f.codes[j]];
      console.assert(code.type === "JS");
      if (code.kind === "Opt") {
        optimized = true;
        if (code.tm >= timeStart && code.tm <= timeEnd) {
          addToCollection(optimizations, code);
        }
      }
      if (code.deopt) {
        deoptimized = true;
        if (code.deopt.tm >= timeStart && code.deopt.tm <= timeEnd) {
          switch (code.deopt.bailoutType) {
            case "lazy":
              addToCollection(lazyDeoptimizations, code);
              break;
            case "eager":
              addToCollection(eagerDeoptimizations, code);
              break;
            case "soft":
              addToCollection(softDeoptimizations, code);
              break;
          }
        }
      }
    }
    if (optimized) {
      optimizedFunctionCount++;
    }
    if (deoptimized) {
      deoptimizedFunctionCount++;
    }
  }

  function sortCollection(collection) {
    collection.functions.sort(
        (a, b) => a.instances.length - b.instances.length);
  }

  sortCollection(eagerDeoptimizations);
  sortCollection(lazyDeoptimizations);
  sortCollection(softDeoptimizations);
  sortCollection(optimizations);

  return {
    functionCount,
    optimizedFunctionCount,
    deoptimizedFunctionCount,
    optimizations,
    eagerDeoptimizations,
    lazyDeoptimizations,
    softDeoptimizations,
  };
}
