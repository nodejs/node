// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Phase, PhaseType } from "./phase";
import { PositionsContainer } from "../position";
import { InstructionsPhase } from "./instructions-phase";

export class SchedulePhase extends Phase {
  data: ScheduleData;
  instructionsPhase: InstructionsPhase;
  positions: PositionsContainer;

  constructor(name: string, dataJson) {
    super(name, PhaseType.Schedule);
    this.data = new ScheduleData();
    this.instructionsPhase = new InstructionsPhase();
    this.positions = new PositionsContainer();
    this.parseScheduleFromJSON(dataJson);
  }

  private parseScheduleFromJSON(dataJson): void {
    const lines = dataJson.split(/[\n]/);
    nextLine:
      for (const line of lines) {
        for (const rule of this.parsingRules) {
          for (const lineRegexp of rule.lineRegexps) {
            const match = line.match(lineRegexp);
            if (match) {
              rule.process(match);
              continue nextLine;
            }
          }
        }
        console.warn(`Unmatched schedule line \"${line}\"`);
      }
  }

  private createNode = match => {
    let inputs = new Array<number>();
    if (match.groups.args) {
      const nodeIdsString = match.groups.args.replace(/\s/g, "");
      const nodeIdStrings = nodeIdsString.split(",");
      inputs = nodeIdStrings.map(n => Number.parseInt(n, 10));
    }
    const nodeId = Number.parseInt(match.groups.id, 10);
    const node = new ScheduleNode(nodeId, match.groups.label, inputs);
    if (match.groups.blocks) {
      const nodeIdsString = match.groups.blocks.replace(/\s/g, "").replace(/B/g, "");
      const nodeIdStrings = nodeIdsString.split(",");
      this.data.lastBlock().successors = nodeIdStrings.map(n => Number.parseInt(n, 10));
    }
    this.data.nodes[node.id] = node;
    this.data.lastBlock().nodes.push(node);
  }

  private createBlock = match => {
    let predecessors = new Array<number>();
    if (match.groups.in) {
      const blockIdsString = match.groups.in.replace(/\s/g, "").replace(/B/g, "");
      const blockIdStrings = blockIdsString.split(",");
      predecessors = blockIdStrings.map(n => Number.parseInt(n, 10));
    }
    const blockId = Number.parseInt(match.groups.id, 10);
    const block = new ScheduleBlock(blockId, match.groups.deferred !== undefined,
      predecessors.sort());
    this.data.blocks[block.id] = block;
  }

  private setGotoSuccessor = match => {
    this.data.lastBlock().successors =
      [Number.parseInt(match.groups.successor.replace(/\s/g, ""), 10)];
  }

  private parsingRules = [
    {
      lineRegexps: [
        /^\s*(?<id>\d+):\ (?<label>.*)\((?<args>.*)\)$/,
        /^\s*(?<id>\d+):\ (?<label>.*)\((?<args>.*)\)\ ->\ (?<blocks>.*)$/,
        /^\s*(?<id>\d+):\ (?<label>.*)$/
      ],
      process: this.createNode
    },
    {
      lineRegexps: [/^\s*---\s*BLOCK\ B(?<id>\d+)\s*(?<deferred>\(deferred\))?(\ <-\ )?(?<in>[^-]*)?\ ---$/],
      process: this.createBlock
    },
    {
      lineRegexps: [/^\s*Goto\s*->\s*B(?<successor>\d+)\s*$/],
      process: this.setGotoSuccessor
    }
  ];
}

export class ScheduleNode {
  id: number;
  label: string;
  inputs: Array<number>;

  constructor(id: number, label: string, inputs: Array<number>) {
    this.id = id;
    this.label = label;
    this.inputs = inputs;
  }

  public toString(): string {
    return `${this.id}: ${this.label}(${this.inputs.join(", ")})`;
  }
}

export class ScheduleBlock {
  id: number;
  deferred: boolean;
  predecessors: Array<number>;
  successors: Array<number>;
  nodes: Array<ScheduleNode>;

  constructor(id: number, deferred: boolean, predecessors: Array<number>) {
    this.id = id;
    this.deferred = deferred;
    this.predecessors = predecessors;
    this.successors = new Array<number>();
    this.nodes = new Array<ScheduleNode>();
  }
}

export class ScheduleData {
  nodes: Array<ScheduleNode>;
  blocks: Array<ScheduleBlock>;

  constructor() {
    this.nodes = new Array<ScheduleNode>();
    this.blocks = new Array<ScheduleBlock>();
  }

  public lastBlock(): ScheduleBlock {
    if (this.blocks.length == 0) return null;
    return this.blocks[this.blocks.length - 1];
  }
}
