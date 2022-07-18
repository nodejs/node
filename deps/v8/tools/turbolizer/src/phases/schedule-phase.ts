// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Phase, PhaseType } from "./phase";

export class SchedulePhase extends Phase {
  data: string;
  schedule: { currentBlock, blocks: Array<any>, nodes: Array<any> };

  constructor(name: string, data: string, schedule?: any) {
    super(name, PhaseType.Schedule);
    this.data = data;
    this.schedule = schedule ?? {
      currentBlock: undefined,
      blocks: new Array<any>(),
      nodes: new Array<any>()
    };
  }

  public parseScheduleFromJSON(scheduleDataJson): void {
    const lines = scheduleDataJson.split(/[\n]/);
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
      this.schedule.currentBlock.succ = nodeIdStrings.map(n => Number.parseInt(n, 10));
    }
    this.schedule.nodes[node.id] = node;
    this.schedule.currentBlock.nodes.push(node);
  }

  private createBlock = match => {
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
    this.schedule.blocks[block.id] = block;
    this.schedule.currentBlock = block;
  }

  private setGotoSuccessor = match => {
    this.schedule.currentBlock.succ = [Number.parseInt(match.groups.successor.replace(/\s/g, ''), 10)];
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
