// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sortUnique, anyToString} from "./util.js"

function sourcePositionLe(a, b) {
  if (a.inliningId == b.inliningId) {
    return a.scriptOffset - b.scriptOffset;
  }
  return a.inliningId - b.inliningId;
}

function sourcePositionEq(a, b) {
  return a.inliningId == b.inliningId &&
    a.scriptOffset == b.scriptOffset;
}

export function sourcePositionToStringKey(sourcePosition): string {
  if (!sourcePosition) return "undefined";
  if (sourcePosition.inliningId && sourcePosition.scriptOffset)
    return "SP:" + sourcePosition.inliningId + ":" + sourcePosition.scriptOffset;
  if (sourcePosition.bytecodePosition)
    return "BCP:" + sourcePosition.bytecodePosition;
  return "undefined";
}

export function sourcePositionValid(l) {
  return (typeof l.scriptOffset !== undefined
    && typeof l.inliningId !== undefined) || typeof l.bytecodePosition != undefined;
}

export interface SourcePosition {
  scriptOffset: number;
  inliningId: number;
}

interface TurboFanOrigin {
  phase: string;
  reducer: string;
}

export interface NodeOrigin {
  nodeId: number;
}

interface BytecodePosition {
  bytecodePosition: number;
}

type Origin = NodeOrigin | BytecodePosition;
type TurboFanNodeOrigin = NodeOrigin & TurboFanOrigin;
type TurboFanBytecodeOrigin = BytecodePosition & TurboFanOrigin;

type AnyPosition = SourcePosition | BytecodePosition;

export interface Source {
  sourcePositions: Array<SourcePosition>;
  sourceName: string;
  functionName: string;
  sourceText: string;
  sourceId: number;
  startPosition?: number;
}
interface Inlining {
  inliningPosition: SourcePosition;
  sourceId: number;
}
interface Phase {
  type: string;
  name: string;
  data: any;
}

export interface Schedule {
  nodes: Array<any>;
}

export interface Sequence {
  blocks: Array<any>;
}

export class SourceResolver {
  nodePositionMap: Array<AnyPosition>;
  sources: Array<Source>;
  inlinings: Array<Inlining>;
  inliningsMap: Map<string, Inlining>;
  positionToNodes: Map<string, Array<string>>;
  phases: Array<Phase>;
  phaseNames: Map<string, number>;
  disassemblyPhase: Phase;
  lineToSourcePositions: Map<string, Array<AnyPosition>>;
  nodeIdToInstructionRange: Array<[number, number]>;
  blockIdToInstructionRange: Array<[number, number]>;
  instructionToPCOffset: Array<number>;
  pcOffsetToInstructions: Map<number, Array<number>>;


  constructor() {
    // Maps node ids to source positions.
    this.nodePositionMap = [];
    // Maps source ids to source objects.
    this.sources = [];
    // Maps inlining ids to inlining objects.
    this.inlinings = [];
    // Maps source position keys to inlinings.
    this.inliningsMap = new Map();
    // Maps source position keys to node ids.
    this.positionToNodes = new Map();
    // Maps phase ids to phases.
    this.phases = [];
    // Maps phase names to phaseIds.
    this.phaseNames = new Map();
    // The disassembly phase is stored separately.
    this.disassemblyPhase = undefined;
    // Maps line numbers to source positions
    this.lineToSourcePositions = new Map();
    // Maps node ids to instruction ranges.
    this.nodeIdToInstructionRange = [];
    // Maps block ids to instruction ranges.
    this.blockIdToInstructionRange = [];
    // Maps instruction numbers to PC offsets.
    this.instructionToPCOffset = [];
    // Maps PC offsets to instructions.
    this.pcOffsetToInstructions = new Map();
  }

  setSources(sources, mainBackup) {
    if (sources) {
      for (let [sourceId, source] of Object.entries(sources)) {
        this.sources[sourceId] = source;
        this.sources[sourceId].sourcePositions = [];
      }
    }
    // This is a fallback if the JSON is incomplete (e.g. due to compiler crash).
    if (!this.sources[-1]) {
      this.sources[-1] = mainBackup;
      this.sources[-1].sourcePositions = [];
    }
  }

  setInlinings(inlinings) {
    if (inlinings) {
      for (const [inliningId, inlining] of Object.entries<Inlining>(inlinings)) {
        this.inlinings[inliningId] = inlining;
        this.inliningsMap.set(sourcePositionToStringKey(inlining.inliningPosition), inlining);
      }
    }
    // This is a default entry for the script itself that helps
    // keep other code more uniform.
    this.inlinings[-1] = { sourceId: -1, inliningPosition: null };
  }

  setNodePositionMap(map) {
    if (!map) return;
    if (typeof map[0] != 'object') {
      const alternativeMap = {};
      for (const [nodeId, scriptOffset] of Object.entries<number>(map)) {
        alternativeMap[nodeId] = { scriptOffset: scriptOffset, inliningId: -1 };
      }
      map = alternativeMap;
    };

    for (const [nodeId, sourcePosition] of Object.entries<SourcePosition>(map)) {
      if (sourcePosition == undefined) {
        console.log("Warning: undefined source position ", sourcePosition, " for nodeId ", nodeId);
      }
      const inliningId = sourcePosition.inliningId;
      const inlining = this.inlinings[inliningId];
      if (inlining) {
        const sourceId = inlining.sourceId;
        this.sources[sourceId].sourcePositions.push(sourcePosition);
      }
      this.nodePositionMap[nodeId] = sourcePosition;
      let key = sourcePositionToStringKey(sourcePosition);
      if (!this.positionToNodes.has(key)) {
        this.positionToNodes.set(key, []);
      }
      this.positionToNodes.get(key).push(nodeId);
    }
    for (const [sourceId, source] of Object.entries(this.sources)) {
      source.sourcePositions = sortUnique(source.sourcePositions,
        sourcePositionLe, sourcePositionEq);
    }
  }

  sourcePositionsToNodeIds(sourcePositions) {
    const nodeIds = new Set();
    for (const sp of sourcePositions) {
      let key = sourcePositionToStringKey(sp);
      let nodeIdsForPosition = this.positionToNodes.get(key);
      if (!nodeIdsForPosition) continue;
      for (const nodeId of nodeIdsForPosition) {
        nodeIds.add(nodeId);
      }
    }
    return nodeIds;
  }

  nodeIdsToSourcePositions(nodeIds): Array<AnyPosition> {
    const sourcePositions = new Map();
    for (const nodeId of nodeIds) {
      let sp = this.nodePositionMap[nodeId];
      let key = sourcePositionToStringKey(sp);
      sourcePositions.set(key, sp);
    }
    const sourcePositionArray = [];
    for (const sp of sourcePositions.values()) {
      sourcePositionArray.push(sp);
    }
    return sourcePositionArray;
  }

  forEachSource(f) {
    this.sources.forEach(f);
  }

  translateToSourceId(sourceId, location) {
    for (const position of this.getInlineStack(location)) {
      let inlining = this.inlinings[position.inliningId];
      if (!inlining) continue;
      if (inlining.sourceId == sourceId) {
        return position;
      }
    }
    return location;
  }

  addInliningPositions(sourcePosition, locations) {
    let inlining = this.inliningsMap.get(sourcePositionToStringKey(sourcePosition));
    if (!inlining) return;
    let sourceId = inlining.sourceId
    const source = this.sources[sourceId];
    for (const sp of source.sourcePositions) {
      locations.push(sp);
      this.addInliningPositions(sp, locations);
    }
  }

  getInliningForPosition(sourcePosition) {
    return this.inliningsMap.get(sourcePositionToStringKey(sourcePosition));
  }

  getSource(sourceId) {
    return this.sources[sourceId];
  }

  getSourceName(sourceId) {
    const source = this.sources[sourceId];
    return `${source.sourceName}:${source.functionName}`;
  }

  sourcePositionFor(sourceId, scriptOffset) {
    if (!this.sources[sourceId]) {
      return null;
    }
    const list = this.sources[sourceId].sourcePositions;
    for (let i = 0; i < list.length; i++) {
      const sourcePosition = list[i]
      const position = sourcePosition.scriptOffset;
      const nextPosition = list[Math.min(i + 1, list.length - 1)].scriptOffset;
      if ((position <= scriptOffset && scriptOffset < nextPosition)) {
        return sourcePosition;
      }
    }
    return null;
  }

  sourcePositionsInRange(sourceId, start, end) {
    if (!this.sources[sourceId]) return [];
    const res = [];
    const list = this.sources[sourceId].sourcePositions;
    for (let i = 0; i < list.length; i++) {
      const sourcePosition = list[i]
      if (start <= sourcePosition.scriptOffset && sourcePosition.scriptOffset < end) {
        res.push(sourcePosition);
      }
    }
    return res;
  }

  getInlineStack(sourcePosition) {
    if (!sourcePosition) {
      return [];
    }
    let inliningStack = [];
    let cur = sourcePosition;
    while (cur && cur.inliningId != -1) {
      inliningStack.push(cur);
      let inlining = this.inlinings[cur.inliningId];
      if (!inlining) {
        break;
      }
      cur = inlining.inliningPosition;
    }
    if (cur && cur.inliningId == -1) {
      inliningStack.push(cur);
    }
    return inliningStack;
  }

  recordOrigins(phase) {
    if (phase.type != "graph") return;
    for (const node of phase.data.nodes) {
      if (node.origin != undefined &&
        node.origin.bytecodePosition != undefined) {
        const position = { bytecodePosition: node.origin.bytecodePosition };
        this.nodePositionMap[node.id] = position;
        let key = sourcePositionToStringKey(position);
        if (!this.positionToNodes.has(key)) {
          this.positionToNodes.set(key, []);
        }
        const A = this.positionToNodes.get(key);
        if (!A.includes(node.id)) A.push("" + node.id);
      }
    }
  }

  readNodeIdToInstructionRange(nodeIdToInstructionRange) {
    for (const [nodeId, range] of Object.entries<[number, number]>(nodeIdToInstructionRange)) {
      this.nodeIdToInstructionRange[nodeId] = range;
    }
  }

  readBlockIdToInstructionRange(blockIdToInstructionRange) {
    for (const [blockId, range] of Object.entries<[number, number]>(blockIdToInstructionRange)) {
      this.blockIdToInstructionRange[blockId] = range;
    }
  }

  getInstruction(nodeId):[number, number] {
    const X = this.nodeIdToInstructionRange[nodeId];
    if (X === undefined) return [-1, -1];
    return X;
  }

  getInstructionRangeForBlock(blockId):[number, number] {
    const X = this.blockIdToInstructionRange[blockId];
    if (X === undefined) return [-1, -1];
    return X;
  }

  readInstructionOffsetToPCOffset(instructionToPCOffset) {
    for (const [instruction, offset] of Object.entries<number>(instructionToPCOffset)) {
      this.instructionToPCOffset[instruction] = offset;
      if (!this.pcOffsetToInstructions.has(offset)) {
        this.pcOffsetToInstructions.set(offset, []);
      }
      this.pcOffsetToInstructions.get(offset).push(instruction);
    }
    console.log(this.pcOffsetToInstructions);
  }

  hasPCOffsets() {
    return this.pcOffsetToInstructions.size > 0;
  }


  nodesForPCOffset(offset): [Array<String>, Array<String>] {
    const keys = Array.from(this.pcOffsetToInstructions.keys()).sort((a, b) => b - a);
    if (keys.length === 0) return [[],[]];
    for (const key of keys) {
      if (key <= offset) {
        const instrs = this.pcOffsetToInstructions.get(key);
        const nodes = [];
        const blocks = [];
        for (const instr of instrs) {
          for (const [nodeId, range] of this.nodeIdToInstructionRange.entries()) {
            if (!range) continue;
            const [start, end] = range;
            if (start == end && instr == start) {
              nodes.push("" + nodeId);
            }
            if (start <= instr && instr < end) {
              nodes.push("" + nodeId);
            }
          }
        }
        return [nodes, blocks];
      }
    }
    return [[],[]];
  }

  parsePhases(phases) {
    for (const [phaseId, phase] of Object.entries<Phase>(phases)) {
      if (phase.type == 'disassembly') {
        this.disassemblyPhase = phase;
      } else if (phase.type == 'schedule') {
        this.phases.push(this.parseSchedule(phase));
        this.phaseNames.set(phase.name, this.phases.length);
      } else if (phase.type == 'sequence') {
        this.phases.push(this.parseSequence(phase));
        this.phaseNames.set(phase.name, this.phases.length);
      } else if (phase.type == 'instructions') {
        if (phase.nodeIdToInstructionRange) {
          this.readNodeIdToInstructionRange(phase.nodeIdToInstructionRange);
        }
        if (phase.blockIdtoInstructionRange) {
          this.readBlockIdToInstructionRange(phase.blockIdtoInstructionRange);
        }
        if (phase.instructionOffsetToPCOffset) {
          this.readInstructionOffsetToPCOffset(phase.instructionOffsetToPCOffset);
        }
      } else {
        this.phases.push(phase);
        this.recordOrigins(phase);
        this.phaseNames.set(phase.name, this.phases.length);
      }
    }
  }

  repairPhaseId(anyPhaseId) {
    return Math.max(0, Math.min(anyPhaseId, this.phases.length - 1))
  }

  getPhase(phaseId) {
    return this.phases[phaseId];
  }

  getPhaseIdByName(phaseName) {
    return this.phaseNames.get(phaseName);
  }

  forEachPhase(f) {
    this.phases.forEach(f);
  }

  addAnyPositionToLine(lineNumber: number | String, sourcePosition: AnyPosition) {
    const lineNumberString = anyToString(lineNumber);
    if (!this.lineToSourcePositions.has(lineNumberString)) {
      this.lineToSourcePositions.set(lineNumberString, []);
    }
    const A = this.lineToSourcePositions.get(lineNumberString);
    if (!A.includes(sourcePosition)) A.push(sourcePosition);
  }

  setSourceLineToBytecodePosition(sourceLineToBytecodePosition: Array<number> | undefined) {
    if (!sourceLineToBytecodePosition) return;
    sourceLineToBytecodePosition.forEach((pos, i) => {
      this.addAnyPositionToLine(i, { bytecodePosition: pos });
    });
  }

  linetoSourcePositions(lineNumber: number | String) {
    const positions = this.lineToSourcePositions.get(anyToString(lineNumber));
    if (positions === undefined) return [];
    return positions;
  }

  parseSchedule(phase) {
    function createNode(state, match) {
      let inputs = [];
      if (match.groups.args) {
        const nodeIdsString = match.groups.args.replace(/\s/g, '');
        const nodeIdStrings = nodeIdsString.split(',');
        inputs = nodeIdStrings.map((n) => Number.parseInt(n, 10));
      }
      const node = {
        id: Number.parseInt(match.groups.id, 10),
        label: match.groups.label,
        inputs: inputs
      };
      if (match.groups.blocks) {
        const nodeIdsString = match.groups.blocks.replace(/\s/g, '').replace(/B/g, '');
        const nodeIdStrings = nodeIdsString.split(',');
        const successors = nodeIdStrings.map((n) => Number.parseInt(n, 10));
        state.currentBlock.succ = successors;
      }
      state.nodes[node.id] = node;
      state.currentBlock.nodes.push(node);
    }
    function createBlock(state, match) {
      let predecessors = [];
      if (match.groups.in) {
        const blockIdsString = match.groups.in.replace(/\s/g, '').replace(/B/g, '');
        const blockIdStrings = blockIdsString.split(',');
        predecessors = blockIdStrings.map((n) => Number.parseInt(n, 10));
      }
      const block = {
        id: Number.parseInt(match.groups.id, 10),
        isDeferred: match.groups.deferred != undefined,
        pred: predecessors.sort(),
        succ: [],
        nodes: []
      };
      state.blocks[block.id] = block;
      state.currentBlock = block;
    }
    function setGotoSuccessor(state, match) {
      state.currentBlock.succ = [Number.parseInt(match.groups.successor.replace(/\s/g, ''), 10)];
    }
    const rules = [
      {
        lineRegexps:
          [/^\s*(?<id>\d+):\ (?<label>.*)\((?<args>.*)\)$/,
            /^\s*(?<id>\d+):\ (?<label>.*)\((?<args>.*)\)\ ->\ (?<blocks>.*)$/,
            /^\s*(?<id>\d+):\ (?<label>.*)$/
          ],
        process: createNode
      },
      {
        lineRegexps:
          [/^\s*---\s*BLOCK\ B(?<id>\d+)\s*(?<deferred>\(deferred\))?(\ <-\ )?(?<in>[^-]*)?\ ---$/
          ],
        process: createBlock
      },
      {
        lineRegexps:
          [/^\s*Goto\s*->\s*B(?<successor>\d+)\s*$/
          ],
        process: setGotoSuccessor
      }
    ];

    const lines = phase.data.split(/[\n]/);
    const state = { currentBlock: undefined, blocks: [], nodes: [] };

    nextLine:
    for (const line of lines) {
      for (const rule of rules) {
        for (const lineRegexp of rule.lineRegexps) {
          const match = line.match(lineRegexp);
          if (match) {
            rule.process(state, match);
            continue nextLine;
          }
        }
      }
      console.log("Warning: unmatched schedule line \"" + line + "\"");
    }
    phase.schedule = state;
    return phase;
  }
  parseSequence(phase) {
    phase.sequence = { blocks: phase.blocks };
    return phase;
  }
}
