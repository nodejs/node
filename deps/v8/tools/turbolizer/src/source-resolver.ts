// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { sortUnique, anyToString } from "../src/util";
import { NodeLabel } from "./node-label";

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

export function sourcePositionToStringKey(sourcePosition: AnyPosition): string {
  if (!sourcePosition) return "undefined";
  if ('inliningId' in sourcePosition && 'scriptOffset' in sourcePosition) {
    return "SP:" + sourcePosition.inliningId + ":" + sourcePosition.scriptOffset;
  }
  if (sourcePosition.bytecodePosition) {
    return "BCP:" + sourcePosition.bytecodePosition;
  }
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

export type Origin = NodeOrigin | BytecodePosition;
export type TurboFanNodeOrigin = NodeOrigin & TurboFanOrigin;
export type TurboFanBytecodeOrigin = BytecodePosition & TurboFanOrigin;

type AnyPosition = SourcePosition | BytecodePosition;

export interface Source {
  sourcePositions: Array<SourcePosition>;
  sourceName: string;
  functionName: string;
  sourceText: string;
  sourceId: number;
  startPosition?: number;
  backwardsCompatibility: boolean;
}
interface Inlining {
  inliningPosition: SourcePosition;
  sourceId: number;
}
interface OtherPhase {
  type: "disassembly" | "sequence" | "schedule";
  name: string;
  data: any;
}

interface InstructionsPhase {
  type: "instructions";
  name: string;
  data: any;
  instructionOffsetToPCOffset?: any;
  blockIdToInstructionRange?: any;
  nodeIdToInstructionRange?: any;
  codeOffsetsInfo?: CodeOffsetsInfo;
}

interface GraphPhase {
  type: "graph";
  name: string;
  data: any;
  highestNodeId: number;
  nodeLabelMap: Array<NodeLabel>;
}

type Phase = GraphPhase | InstructionsPhase | OtherPhase;

export interface Schedule {
  nodes: Array<any>;
}

export class Interval {
  start: number;
  end: number;

  constructor(numbers: [number, number]) {
    this.start = numbers[0];
    this.end = numbers[1];
  }
}

export interface ChildRange {
  id: string;
  type: string;
  op: any;
  intervals: Array<[number, number]>;
  uses: Array<number>;
}

export interface Range {
  child_ranges: Array<ChildRange>;
  is_deferred: boolean;
}

export class RegisterAllocation {
  fixedDoubleLiveRanges: Map<string, Range>;
  fixedLiveRanges: Map<string, Range>;
  liveRanges: Map<string, Range>;

  constructor(registerAllocation) {
    this.fixedDoubleLiveRanges = new Map<string, Range>(Object.entries(registerAllocation.fixed_double_live_ranges));
    this.fixedLiveRanges = new Map<string, Range>(Object.entries(registerAllocation.fixed_live_ranges));
    this.liveRanges = new Map<string, Range>(Object.entries(registerAllocation.live_ranges));
  }
}

export interface Sequence {
  blocks: Array<any>;
  register_allocation: RegisterAllocation;
}

class CodeOffsetsInfo {
  codeStartRegisterCheck: number;
  deoptCheck: number;
  initPoison: number;
  blocksStart: number;
  outOfLineCode: number;
  deoptimizationExits: number;
  pools: number;
  jumpTables: number;
}
export class TurbolizerInstructionStartInfo {
  gap: number;
  arch: number;
  condition: number;
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
  linePositionMap: Map<string, Array<AnyPosition>>;
  nodeIdToInstructionRange: Array<[number, number]>;
  blockIdToInstructionRange: Array<[number, number]>;
  instructionToPCOffset: Array<TurbolizerInstructionStartInfo>;
  pcOffsetToInstructions: Map<number, Array<number>>;
  pcOffsets: Array<number>;
  blockIdToPCOffset: Array<number>;
  blockStartPCtoBlockIds: Map<number, Array<number>>;
  codeOffsetsInfo: CodeOffsetsInfo;

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
    this.linePositionMap = new Map();
    // Maps node ids to instruction ranges.
    this.nodeIdToInstructionRange = [];
    // Maps block ids to instruction ranges.
    this.blockIdToInstructionRange = [];
    // Maps instruction numbers to PC offsets.
    this.instructionToPCOffset = [];
    // Maps PC offsets to instructions.
    this.pcOffsetToInstructions = new Map();
    this.pcOffsets = [];
    this.blockIdToPCOffset = [];
    this.blockStartPCtoBlockIds = new Map();
    this.codeOffsetsInfo = null;
  }

  getBlockIdsForOffset(offset): Array<number> {
    return this.blockStartPCtoBlockIds.get(offset);
  }

  hasBlockStartInfo() {
    return this.blockIdToPCOffset.length > 0;
  }

  setSources(sources, mainBackup) {
    if (sources) {
      for (const [sourceId, source] of Object.entries(sources)) {
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
    }

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
      const key = sourcePositionToStringKey(sourcePosition);
      if (!this.positionToNodes.has(key)) {
        this.positionToNodes.set(key, []);
      }
      this.positionToNodes.get(key).push(nodeId);
    }
    for (const [, source] of Object.entries(this.sources)) {
      source.sourcePositions = sortUnique(source.sourcePositions,
        sourcePositionLe, sourcePositionEq);
    }
  }

  sourcePositionsToNodeIds(sourcePositions) {
    const nodeIds = new Set<string>();
    for (const sp of sourcePositions) {
      const key = sourcePositionToStringKey(sp);
      const nodeIdsForPosition = this.positionToNodes.get(key);
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
      const sp = this.nodePositionMap[nodeId];
      const key = sourcePositionToStringKey(sp);
      sourcePositions.set(key, sp);
    }
    const sourcePositionArray = [];
    for (const sp of sourcePositions.values()) {
      sourcePositionArray.push(sp);
    }
    return sourcePositionArray;
  }

  forEachSource(f: (value: Source, index: number, array: Array<Source>) => void) {
    this.sources.forEach(f);
  }

  translateToSourceId(sourceId: number, location?: SourcePosition) {
    for (const position of this.getInlineStack(location)) {
      const inlining = this.inlinings[position.inliningId];
      if (!inlining) continue;
      if (inlining.sourceId == sourceId) {
        return position;
      }
    }
    return location;
  }

  addInliningPositions(sourcePosition: AnyPosition, locations: Array<SourcePosition>) {
    const inlining = this.inliningsMap.get(sourcePositionToStringKey(sourcePosition));
    if (!inlining) return;
    const sourceId = inlining.sourceId;
    const source = this.sources[sourceId];
    for (const sp of source.sourcePositions) {
      locations.push(sp);
      this.addInliningPositions(sp, locations);
    }
  }

  getInliningForPosition(sourcePosition: AnyPosition) {
    return this.inliningsMap.get(sourcePositionToStringKey(sourcePosition));
  }

  getSource(sourceId: number) {
    return this.sources[sourceId];
  }

  getSourceName(sourceId: number) {
    const source = this.sources[sourceId];
    return `${source.sourceName}:${source.functionName}`;
  }

  sourcePositionFor(sourceId: number, scriptOffset: number) {
    if (!this.sources[sourceId]) {
      return null;
    }
    const list = this.sources[sourceId].sourcePositions;
    for (let i = 0; i < list.length; i++) {
      const sourcePosition = list[i];
      const position = sourcePosition.scriptOffset;
      const nextPosition = list[Math.min(i + 1, list.length - 1)].scriptOffset;
      if ((position <= scriptOffset && scriptOffset < nextPosition)) {
        return sourcePosition;
      }
    }
    return null;
  }

  sourcePositionsInRange(sourceId: number, start: number, end: number) {
    if (!this.sources[sourceId]) return [];
    const res = [];
    const list = this.sources[sourceId].sourcePositions;
    for (const sourcePosition of list) {
      if (start <= sourcePosition.scriptOffset && sourcePosition.scriptOffset < end) {
        res.push(sourcePosition);
      }
    }
    return res;
  }

  getInlineStack(sourcePosition?: SourcePosition) {
    if (!sourcePosition) return [];

    const inliningStack = [];
    let cur = sourcePosition;
    while (cur && cur.inliningId != -1) {
      inliningStack.push(cur);
      const inlining = this.inlinings[cur.inliningId];
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

  recordOrigins(phase: GraphPhase) {
    if (phase.type != "graph") return;
    for (const node of phase.data.nodes) {
      phase.highestNodeId = Math.max(phase.highestNodeId, node.id);
      if (node.origin != undefined &&
        node.origin.bytecodePosition != undefined) {
        const position = { bytecodePosition: node.origin.bytecodePosition };
        this.nodePositionMap[node.id] = position;
        const key = sourcePositionToStringKey(position);
        if (!this.positionToNodes.has(key)) {
          this.positionToNodes.set(key, []);
        }
        const A = this.positionToNodes.get(key);
        if (!A.includes(node.id)) A.push(`${node.id}`);
      }

      // Backwards compatibility.
      if (typeof node.pos === "number") {
        node.sourcePosition = { scriptOffset: node.pos, inliningId: -1 };
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

  getInstruction(nodeId: number): [number, number] {
    const X = this.nodeIdToInstructionRange[nodeId];
    if (X === undefined) return [-1, -1];
    return X;
  }

  getInstructionRangeForBlock(blockId: number): [number, number] {
    const X = this.blockIdToInstructionRange[blockId];
    if (X === undefined) return [-1, -1];
    return X;
  }

  readInstructionOffsetToPCOffset(instructionToPCOffset) {
    for (const [instruction, numberOrInfo] of Object.entries<number | TurbolizerInstructionStartInfo>(instructionToPCOffset)) {
      let info: TurbolizerInstructionStartInfo;
      if (typeof numberOrInfo == "number") {
        info = { gap: numberOrInfo, arch: numberOrInfo, condition: numberOrInfo };
      } else {
        info = numberOrInfo;
      }
      this.instructionToPCOffset[instruction] = info;
      if (!this.pcOffsetToInstructions.has(info.gap)) {
        this.pcOffsetToInstructions.set(info.gap, []);
      }
      this.pcOffsetToInstructions.get(info.gap).push(Number(instruction));
    }
    this.pcOffsets = Array.from(this.pcOffsetToInstructions.keys()).sort((a, b) => b - a);
  }

  hasPCOffsets() {
    return this.pcOffsetToInstructions.size > 0;
  }

  getKeyPcOffset(offset: number): number {
    if (this.pcOffsets.length === 0) return -1;
    for (const key of this.pcOffsets) {
      if (key <= offset) {
        return key;
      }
    }
    return -1;
  }

  getInstructionKindForPCOffset(offset: number) {
    if (this.codeOffsetsInfo) {
      if (offset >= this.codeOffsetsInfo.deoptimizationExits) {
        if (offset >= this.codeOffsetsInfo.pools) {
          return "pools";
        } else if (offset >= this.codeOffsetsInfo.jumpTables) {
          return "jump-tables";
        } else {
          return "deoptimization-exits";
        }
      }
      if (offset < this.codeOffsetsInfo.deoptCheck) {
        return "code-start-register";
      } else if (offset < this.codeOffsetsInfo.initPoison) {
        return "deopt-check";
      } else if (offset < this.codeOffsetsInfo.blocksStart) {
        return "init-poison";
      }
    }
    const keyOffset = this.getKeyPcOffset(offset);
    if (keyOffset != -1) {
      const infos = this.pcOffsetToInstructions.get(keyOffset).map(instrId => this.instructionToPCOffset[instrId]).filter(info => info.gap != info.condition);
      if (infos.length > 0) {
        const info = infos[0];
        if (!info || info.gap == info.condition) return "unknown";
        if (offset < info.arch) return "gap";
        if (offset < info.condition) return "arch";
        return "condition";
      }
    }
    return "unknown";
  }

  instructionKindToReadableName(instructionKind) {
    switch (instructionKind) {
      case "code-start-register": return "Check code register for right value";
      case "deopt-check": return "Check if function was marked for deoptimization";
      case "init-poison": return "Initialization of poison register";
      case "gap": return "Instruction implementing a gap move";
      case "arch": return "Instruction implementing the actual machine operation";
      case "condition": return "Code implementing conditional after instruction";
      case "pools": return "Data in a pool (e.g. constant pool)";
      case "jump-tables": return "Part of a jump table";
      case "deoptimization-exits": return "Jump to deoptimization exit";
    }
    return null;
  }

  instructionRangeToKeyPcOffsets([start, end]: [number, number]): Array<TurbolizerInstructionStartInfo> {
    if (start == end) return [this.instructionToPCOffset[start]];
    return this.instructionToPCOffset.slice(start, end);
  }

  instructionToPcOffsets(instr: number): TurbolizerInstructionStartInfo {
    return this.instructionToPCOffset[instr];
  }

  instructionsToKeyPcOffsets(instructionIds: Iterable<number>): Array<number> {
    const keyPcOffsets = [];
    for (const instructionId of instructionIds) {
      const pcOffset = this.instructionToPCOffset[instructionId];
      if (pcOffset !== undefined) keyPcOffsets.push(pcOffset.gap);
    }
    return keyPcOffsets;
  }

  nodesToKeyPcOffsets(nodes) {
    let offsets = [];
    for (const node of nodes) {
      const range = this.nodeIdToInstructionRange[node];
      if (!range) continue;
      offsets = offsets.concat(this.instructionRangeToKeyPcOffsets(range));
    }
    return offsets;
  }

  nodesForPCOffset(offset: number): [Array<string>, Array<string>] {
    if (this.pcOffsets.length === 0) return [[], []];
    for (const key of this.pcOffsets) {
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
    return [[], []];
  }

  parsePhases(phases) {
    const nodeLabelMap = [];
    for (const [, phase] of Object.entries<Phase>(phases)) {
      switch (phase.type) {
        case 'disassembly':
          this.disassemblyPhase = phase;
          if (phase['blockIdToOffset']) {
            for (const [blockId, pc] of Object.entries<number>(phase['blockIdToOffset'])) {
              this.blockIdToPCOffset[blockId] = pc;
              if (!this.blockStartPCtoBlockIds.has(pc)) {
                this.blockStartPCtoBlockIds.set(pc, []);
              }
              this.blockStartPCtoBlockIds.get(pc).push(Number(blockId));
            }
          }
          break;
        case 'schedule':
          this.phaseNames.set(phase.name, this.phases.length);
          this.phases.push(this.parseSchedule(phase));
          break;
        case 'sequence':
          this.phaseNames.set(phase.name, this.phases.length);
          this.phases.push(this.parseSequence(phase));
          break;
        case 'instructions':
          if (phase.nodeIdToInstructionRange) {
            this.readNodeIdToInstructionRange(phase.nodeIdToInstructionRange);
          }
          if (phase.blockIdToInstructionRange) {
            this.readBlockIdToInstructionRange(phase.blockIdToInstructionRange);
          }
          if (phase.instructionOffsetToPCOffset) {
            this.readInstructionOffsetToPCOffset(phase.instructionOffsetToPCOffset);
          }
          if (phase.codeOffsetsInfo) {
            this.codeOffsetsInfo = phase.codeOffsetsInfo;
          }
          break;
        case 'graph':
          const graphPhase: GraphPhase = Object.assign(phase, { highestNodeId: 0 });
          this.phaseNames.set(graphPhase.name, this.phases.length);
          this.phases.push(graphPhase);
          this.recordOrigins(graphPhase);
          this.internNodeLabels(graphPhase, nodeLabelMap);
          graphPhase.nodeLabelMap = nodeLabelMap.slice();
          break;
        default:
          throw "Unsupported phase type";
      }
    }
  }

  internNodeLabels(phase: GraphPhase, nodeLabelMap: Array<NodeLabel>) {
    for (const n of phase.data.nodes) {
      const label = new NodeLabel(n.id, n.label, n.title, n.live,
        n.properties, n.sourcePosition, n.origin, n.opcode, n.control,
        n.opinfo, n.type);
      const previous = nodeLabelMap[label.id];
      if (!label.equals(previous)) {
        if (previous != undefined) {
          label.setInplaceUpdatePhase(phase.name);
        }
        nodeLabelMap[label.id] = label;
      }
      n.nodeLabel = nodeLabelMap[label.id];
    }
  }

  repairPhaseId(anyPhaseId) {
    return Math.max(0, Math.min(anyPhaseId | 0, this.phases.length - 1));
  }

  getPhase(phaseId: number) {
    return this.phases[phaseId];
  }

  getPhaseIdByName(phaseName: string) {
    return this.phaseNames.get(phaseName);
  }

  forEachPhase(f: (value: Phase, index: number, array: Array<Phase>) => void) {
    this.phases.forEach(f);
  }

  addAnyPositionToLine(lineNumber: number | string, sourcePosition: AnyPosition) {
    const lineNumberString = anyToString(lineNumber);
    if (!this.linePositionMap.has(lineNumberString)) {
      this.linePositionMap.set(lineNumberString, []);
    }
    const A = this.linePositionMap.get(lineNumberString);
    if (!A.includes(sourcePosition)) A.push(sourcePosition);
  }

  setSourceLineToBytecodePosition(sourceLineToBytecodePosition: Array<number> | undefined) {
    if (!sourceLineToBytecodePosition) return;
    sourceLineToBytecodePosition.forEach((pos, i) => {
      this.addAnyPositionToLine(i, { bytecodePosition: pos });
    });
  }

  lineToSourcePositions(lineNumber: number | string) {
    const positions = this.linePositionMap.get(anyToString(lineNumber));
    if (positions === undefined) return [];
    return positions;
  }

  parseSchedule(phase) {
    function createNode(state: any, match) {
      let inputs = [];
      if (match.groups.args) {
        const nodeIdsString = match.groups.args.replace(/\s/g, '');
        const nodeIdStrings = nodeIdsString.split(',');
        inputs = nodeIdStrings.map(n => Number.parseInt(n, 10));
      }
      const node = {
        id: Number.parseInt(match.groups.id, 10),
        label: match.groups.label,
        inputs: inputs
      };
      if (match.groups.blocks) {
        const nodeIdsString = match.groups.blocks.replace(/\s/g, '').replace(/B/g, '');
        const nodeIdStrings = nodeIdsString.split(',');
        const successors = nodeIdStrings.map(n => Number.parseInt(n, 10));
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
        predecessors = blockIdStrings.map(n => Number.parseInt(n, 10));
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
    phase.sequence = { blocks: phase.blocks,
                       register_allocation: phase.register_allocation ? new RegisterAllocation(phase.register_allocation)
                                                                      : undefined };
    return phase;
  }
}
