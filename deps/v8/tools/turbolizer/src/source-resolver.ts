// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { anyToString, camelize, sortUnique } from "./common/util";
import { PhaseType } from "./phases/phase";
import { GraphPhase } from "./phases/graph-phase/graph-phase";
import { DisassemblyPhase } from "./phases/disassembly-phase";
import { BytecodePosition, InliningPosition, SourcePosition } from "./position";
import { InstructionsPhase } from "./phases/instructions-phase";
import { SchedulePhase } from "./phases/schedule-phase";
import { SequencePhase } from "./phases/sequence-phase";
import { BytecodeOrigin } from "./origin";
import { Source } from "./source";
import { NodeLabel } from "./node-label";
import { TurboshaftGraphPhase } from "./phases/turboshaft-graph-phase/turboshaft-graph-phase";

export type GenericPosition = SourcePosition | BytecodePosition;
export type GenericPhase = GraphPhase | TurboshaftGraphPhase | DisassemblyPhase
  | InstructionsPhase | SchedulePhase | SequencePhase;

export class SourceResolver {
  nodePositionMap: Array<GenericPosition>;
  sources: Array<Source>;
  inlinings: Array<InliningPosition>;
  inliningsMap: Map<string, InliningPosition>;
  positionToNodes: Map<string, Array<string>>;
  phases: Array<GenericPhase>;
  phaseNames: Map<string, number>;
  disassemblyPhase: DisassemblyPhase;
  instructionsPhase: InstructionsPhase;
  linePositionMap: Map<string, Array<GenericPosition>>;

  constructor() {
    // Maps node ids to source positions.
    this.nodePositionMap = new Array<GenericPosition>();
    // Maps source ids to source objects.
    this.sources = new Array<Source>();
    // Maps inlining ids to inlining objects.
    this.inlinings = new Array<InliningPosition>();
    // Maps source position keys to inlinings.
    this.inliningsMap = new Map<string, InliningPosition>();
    // Maps source position keys to node ids.
    this.positionToNodes = new Map<string, Array<string>>();
    // Maps phase ids to phases.
    this.phases = new Array<GenericPhase>();
    // Maps phase names to phaseIds.
    this.phaseNames = new Map<string, number>();
    // The disassembly phase is stored separately.
    this.disassemblyPhase = undefined;
    // Maps line numbers to source positions
    this.linePositionMap = new Map<string, Array<GenericPosition>>();
  }

  public getMainFunction(jsonObj): Source {
    const fncJson = jsonObj.function;
    // Backwards compatibility.
    if (typeof fncJson === "string") {
      return new Source(null, null, jsonObj.source, -1, true,
        new Array<SourcePosition>(), jsonObj.sourcePosition,
        jsonObj.sourcePosition + jsonObj.source.length);
    }
    return new Source(fncJson.sourceName, fncJson.functionName, fncJson.sourceText,
      fncJson.sourceId, false, new Array<SourcePosition>(), fncJson.startPosition,
      fncJson.endPosition);
  }

  public setInlinings(inliningsJson): void {
    if (inliningsJson) {
      for (const [inliningIdStr, inlining] of Object.entries<InliningPosition>(inliningsJson)) {
        const scriptOffset = inlining.inliningPosition.scriptOffset;
        const inliningId = inlining.inliningPosition.inliningId;
        const inl = new InliningPosition(inlining.sourceId,
          new SourcePosition(scriptOffset, inliningId));
        this.inlinings[inliningIdStr] = inl;
        this.inliningsMap.set(inl.inliningPosition.toString(), inl);
      }
    }
    // This is a default entry for the script itself that helps
    // keep other code more uniform.
    this.inlinings[-1] = new InliningPosition(-1, null);
  }

  public setSources(sourcesJson, mainFunc: Source): void {
    if (sourcesJson) {
      for (const [sourceId, source] of Object.entries<Source>(sourcesJson)) {
        const src = new Source(source.sourceName, source.functionName, source.sourceText,
          source.sourceId, source.backwardsCompatibility, new Array<SourcePosition>(),
          source.startPosition, source.endPosition);
        this.sources[sourceId] = src;
      }
    }
    // This is a fallback if the JSON is incomplete (e.g. due to compiler crash).
    if (!this.sources[-1]) {
      this.sources[-1] = mainFunc;
    }
  }

  public setNodePositionMap(mapJson): void {
    if (!mapJson) return;
    if (typeof mapJson[0] !== "object") {
      const alternativeMap = new Map<string, SourcePosition>();
      for (const [nodeId, scriptOffset] of Object.entries<number>(mapJson)) {
        alternativeMap[nodeId] = new SourcePosition(scriptOffset, -1);
      }
      mapJson = alternativeMap;
    }

    for (const [nodeId, sourcePosition] of Object.entries<SourcePosition>(mapJson)) {
      if (sourcePosition === undefined) {
        console.warn(`Undefined source position for node id ${nodeId}`);
      }
      const inlining = this.inlinings[sourcePosition.inliningId];
      const sp = new SourcePosition(sourcePosition.scriptOffset, sourcePosition.inliningId);
      if (inlining) this.sources[inlining.sourceId].sourcePositions.push(sp);
      this.nodePositionMap[nodeId] = sp;
      const key = sp.toString();
      if (!this.positionToNodes.has(key)) {
        this.positionToNodes.set(key, new Array<string>());
      }
      this.positionToNodes.get(key).push(nodeId);
    }

    for (const [, source] of Object.entries<Source>(this.sources)) {
      source.sourcePositions = sortUnique(source.sourcePositions,
        (a, b) => a.lessOrEquals(b),
        (a, b) => a.equals(b));
    }
  }

  public parsePhases(phasesJson): void {
    const nodeLabelMap = new Array<NodeLabel>();
    for (const [, genericPhase] of Object.entries<GenericPhase>(phasesJson)) {
      switch (genericPhase.type) {
        case PhaseType.Disassembly:
          const castedDisassembly = genericPhase as DisassemblyPhase;
          const disassemblyPhase = new DisassemblyPhase(castedDisassembly.name,
            castedDisassembly.data);
          disassemblyPhase.parseBlockIdToOffsetFromJSON(castedDisassembly?.blockIdToOffset);
          this.disassemblyPhase = disassemblyPhase;
          break;
        case PhaseType.Schedule:
          const castedSchedule = genericPhase as SchedulePhase;
          const schedulePhase = new SchedulePhase(castedSchedule.name, castedSchedule.data);
          this.phaseNames.set(schedulePhase.name, this.phases.length);
          schedulePhase.parseScheduleFromJSON(castedSchedule.data);
          this.phases.push(schedulePhase);
          break;
        case PhaseType.Sequence:
          const castedSequence = camelize(genericPhase) as SequencePhase;
          const sequencePhase = new SequencePhase(castedSequence.name, castedSequence.blocks,
            castedSequence.registerAllocation);
          this.phaseNames.set(sequencePhase.name, this.phases.length);
          this.phases.push(sequencePhase);
          break;
        case PhaseType.Instructions:
          const castedInstructions = genericPhase as InstructionsPhase;
          let instructionsPhase: InstructionsPhase = null;
          if (this.instructionsPhase) {
            instructionsPhase = this.instructionsPhase;
            instructionsPhase.name += `, ${castedInstructions.name}`;
          } else {
            instructionsPhase = new InstructionsPhase(castedInstructions.name);
          }
          instructionsPhase.parseNodeIdToInstructionRangeFromJSON(castedInstructions
            ?.nodeIdToInstructionRange);
          instructionsPhase.parseBlockIdToInstructionRangeFromJSON(castedInstructions
            ?.blockIdToInstructionRange);
          instructionsPhase.parseInstructionOffsetToPCOffsetFromJSON(castedInstructions
            ?.instructionOffsetToPCOffset);
          instructionsPhase.parseCodeOffsetsInfoFromJSON(castedInstructions
            ?.codeOffsetsInfo);
          this.instructionsPhase = instructionsPhase;
          break;
        case PhaseType.Graph:
          const castedGraph = genericPhase as GraphPhase;
          const graphPhase = new GraphPhase(castedGraph.name, 0);
          graphPhase.parseDataFromJSON(castedGraph.data, nodeLabelMap);
          graphPhase.nodeLabelMap = nodeLabelMap.slice();
          this.recordOrigins(graphPhase);
          this.phaseNames.set(graphPhase.name, this.phases.length);
          this.phases.push(graphPhase);
          break;
        case PhaseType.TurboshaftGraph:
          const castedTurboshaftGraph = genericPhase as TurboshaftGraphPhase;
          const turboshaftGraphPhase = new TurboshaftGraphPhase(castedTurboshaftGraph.name, 0);
          turboshaftGraphPhase.parseDataFromJSON(castedTurboshaftGraph.data);
          this.phaseNames.set(turboshaftGraphPhase.name, this.phases.length);
          this.phases.push(turboshaftGraphPhase);
          break;
        default:
          throw "Unsupported phase type";
      }
    }
  }

  public sourcePositionsToNodeIds(sourcePositions: Array<GenericPosition>): Set<string> {
    const nodeIds = new Set<string>();
    for (const sp of sourcePositions) {
      const nodeIdsForPosition = this.positionToNodes.get(sp.toString());
      if (!nodeIdsForPosition) continue;
      for (const nodeId of nodeIdsForPosition) {
        nodeIds.add(nodeId);
      }
    }
    return nodeIds;
  }

  public nodeIdsToSourcePositions(nodeIds: Array<string>): Array<GenericPosition> {
    const sourcePositions = new Map<string, GenericPosition>();
    for (const nodeId of nodeIds) {
      const position = this.nodePositionMap[nodeId];
      if (!position) continue;
      sourcePositions.set(position.toString(), position);
    }
    const sourcePositionArray = new Array<GenericPosition>();
    for (const sourcePosition of sourcePositions.values()) {
      sourcePositionArray.push(sourcePosition);
    }
    return sourcePositionArray;
  }

  public translateToSourceId(sourceId: number, location?: SourcePosition): SourcePosition {
    for (const position of this.getInlineStack(location)) {
      const inlining = this.inlinings[position.inliningId];
      if (!inlining) continue;
      if (inlining.sourceId == sourceId) {
        return position;
      }
    }
    return location;
  }

  public addInliningPositions(sourcePosition: GenericPosition, locations: Array<SourcePosition>):
    void {
    const inlining = this.inliningsMap.get(sourcePosition.toString());
    if (!inlining) return;
    const source = this.sources[inlining.sourceId];
    for (const sp of source.sourcePositions) {
      locations.push(sp);
      this.addInliningPositions(sp, locations);
    }
  }

  public getInliningForPosition(sourcePosition: GenericPosition): InliningPosition {
    return this.inliningsMap.get(sourcePosition.toString());
  }

  public getSource(sourceId: number): Source {
    return this.sources[sourceId];
  }

  public addAnyPositionToLine(lineNumber: number | string, sourcePosition: GenericPosition): void {
    const lineNumberString = anyToString(lineNumber);
    if (!this.linePositionMap.has(lineNumberString)) {
      this.linePositionMap.set(lineNumberString, new Array<GenericPosition>());
    }
    const storedPositions = this.linePositionMap.get(lineNumberString);
    if (!storedPositions.includes(sourcePosition)) storedPositions.push(sourcePosition);
  }

  public repairPhaseId(anyPhaseId: number): number {
    return Math.max(0, Math.min(anyPhaseId | 0, this.phases.length - 1));
  }

  public getPhase(phaseId: number): GenericPhase {
    return this.phases[phaseId];
  }

  public getPhaseIdByName(phaseName: string): number {
    return this.phaseNames.get(phaseName);
  }

  public lineToSourcePositions(lineNumber: number | string): Array<GenericPosition> {
    return this.linePositionMap.get(anyToString(lineNumber)) ?? new Array<GenericPosition>();
  }

  public getSourceName(sourceId: number): string {
    const source = this.sources[sourceId];
    return `${source.sourceName}:${source.functionName}`;
  }

  public sourcePositionsInRange(sourceId: number, start: number, end: number):
    Array<SourcePosition> {
    const inRange = Array<SourcePosition>();
    if (!this.sources[sourceId]) return inRange;
    const list = this.sources[sourceId].sourcePositions;
    for (const sourcePosition of list) {
      if (start <= sourcePosition.scriptOffset && sourcePosition.scriptOffset < end) {
        inRange.push(sourcePosition);
      }
    }
    return inRange;
  }

  public setSourceLineToBytecodePosition(sourceLineToBytecodePositionJson): void {
    if (!sourceLineToBytecodePositionJson) return;
    sourceLineToBytecodePositionJson.forEach((position, idx) => {
      this.addAnyPositionToLine(idx, new BytecodePosition(position));
    });
  }

  private getInlineStack(sourcePosition?: SourcePosition): Array<SourcePosition> {
    const inliningStack = Array<SourcePosition>();
    if (!sourcePosition) return inliningStack;
    let cur = sourcePosition;
    while (cur && cur.inliningId != -1) {
      inliningStack.push(cur);
      const inlining = this.inlinings[cur.inliningId];
      if (!inlining) break;
      cur = inlining.inliningPosition;
    }
    if (cur && cur.inliningId == -1) {
      inliningStack.push(cur);
    }
    return inliningStack;
  }

  private recordOrigins(graphPhase: GraphPhase): void {
    if (graphPhase.type !== PhaseType.Graph) return;
    for (const node of graphPhase.data.nodes) {
      graphPhase.highestNodeId = Math.max(graphPhase.highestNodeId, node.id);
      const origin = node.nodeLabel.origin;
      if (origin instanceof BytecodeOrigin) {
        const position = new BytecodePosition(origin.bytecodePosition);
        this.nodePositionMap[node.id] = position;
        const key = position.toString();
        if (!this.positionToNodes.has(key)) {
          this.positionToNodes.set(key, new Array<string>());
        }
        const nodes = this.positionToNodes.get(key);
        const identifier = node.identifier();
        if (!nodes.includes(identifier)) nodes.push(identifier);
      }
    }
  }
}
