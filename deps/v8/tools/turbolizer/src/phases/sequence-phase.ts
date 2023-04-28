// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../common/constants";
import { Phase, PhaseType } from "./phase";
import { PositionsContainer } from "../position";
import { InstructionsPhase } from "./instructions-phase";

export class SequencePhase extends Phase {
  blocks: Array<SequenceBlock>;
  instructions: Array<SequenceBlockInstruction>;
  instructionsPhase: InstructionsPhase;
  positions: PositionsContainer;
  registerAllocation: RegisterAllocation;

  constructor(name: string, blocksJSON, registerAllocationJSON) {
    super(name, PhaseType.Sequence);
    this.instructionsPhase = new InstructionsPhase();
    this.positions = new PositionsContainer();
    this.parseBlocksFromJSON(blocksJSON);
    this.parseRegisterAllocationFromJSON(registerAllocationJSON);
  }

  public getNumInstructions(): number {
    const lastBlock = this.lastBlock();
    return lastBlock.instructions[lastBlock.instructions.length - 1].id + 1;
  }

  public lastBlock(): SequenceBlock {
    if (this.blocks.length == 0) return null;
    return this.blocks[this.blocks.length - 1];
  }

  private parseBlocksFromJSON(blocksJSON): void {
    if (!blocksJSON || blocksJSON.length == 0) return;
    this.blocks = new Array<SequenceBlock>();
    this.instructions = new Array<SequenceBlockInstruction>();
    for (const block of blocksJSON) {
      const newBlock = new SequenceBlock(block.id, block.deferred, block.loopHeader, block.loopEnd,
        block.predecessors, block.successors);
      this.blocks.push(newBlock);
      this.parseBlockInstructions(block.instructions);
      this.parseBlockPhis(block.phis);
    }
  }

  private parseBlockInstructions(instructionsJSON): void {
    if (!instructionsJSON || instructionsJSON.length == 0) return;
    for (const instruction of instructionsJSON) {
      const newInstruction = new SequenceBlockInstruction(instruction.id, instruction.opcode,
        instruction.flags, instruction.temps);
      for (const input of instruction.inputs) {
        newInstruction.inputs.push(this.parseOperandFromJSON(input));
      }
      for (const output of instruction.outputs) {
        newInstruction.outputs.push(this.parseOperandFromJSON(output));
      }
      for (const gap of instruction.gaps) {
        const newGap = new Array<[SequenceBlockOperand, SequenceBlockOperand]>();
        for (const [destination, source] of gap) {
          newGap.push([this.parseOperandFromJSON(destination), this.parseOperandFromJSON(source)]);
        }
        newInstruction.gaps.push(newGap);
      }
      this.lastBlock().instructions.push(newInstruction);
      this.instructions.push(newInstruction);
    }
  }

  private parseBlockPhis(phisJSON): void {
    if (!phisJSON || phisJSON.length == 0) return;
    for (const phi of phisJSON) {
      const newBlockPhi = new SequenceBlockPhi(phi.operands, this.parseOperandFromJSON(phi.output));
      this.lastBlock().phis.push(newBlockPhi);
    }
  }

  private parseRegisterAllocationFromJSON(registerAllocationJSON): void {
    if (!registerAllocationJSON) return;
    this.registerAllocation = new RegisterAllocation();
    this.registerAllocation.fixedDoubleLiveRanges =
      this.parseRangesFromJSON(registerAllocationJSON.fixedDoubleLiveRanges);
    this.registerAllocation.fixedLiveRanges =
      this.parseRangesFromJSON(registerAllocationJSON.fixedLiveRanges);
    this.registerAllocation.liveRanges =
      this.parseRangesFromJSON(registerAllocationJSON.liveRanges);
  }

  private parseRangesFromJSON(rangesJSON): Array<Range> {
    if (!rangesJSON) return null;
    const parsedRanges = new Array<Range>();
    for (const [idx, range] of Object.entries<Range>(rangesJSON)) {
      const newRange = new Range(range.isDeferred, range.instructionRange);
      for (const childRange of range.childRanges) {
        let operand: SequenceBlockOperand | string = null;
        if (childRange.op) {
          if (typeof childRange.op === "string") {
            operand = childRange.op;
          } else {
            operand = this.parseOperandFromJSON(childRange.op);
          }
        }

        const newChildRange = new ChildRange(childRange.id, childRange.type,
          operand, childRange.intervals, childRange.uses);
        newRange.childRanges.push(newChildRange);
      }
      parsedRanges[idx] = newRange;
    }
    return parsedRanges;
  }

  private parseOperandFromJSON(operandJSON): SequenceBlockOperand {
    return new SequenceBlockOperand(operandJSON.type, operandJSON.text, operandJSON.tooltip);
  }
}

export class SequenceBlock {
  id: number;
  deferred: boolean;
  loopHeader: number;
  loopEnd: number;
  predecessors: Array<number>;
  successors: Array<number>;
  instructions: Array<SequenceBlockInstruction>;
  phis: Array<SequenceBlockPhi>;

  constructor(id: number, deferred: boolean, loopHeader: number, loopEnd: number,
              predecessors: Array<number>, successors: Array<number>) {
    this.id = id;
    this.deferred = deferred;
    this.loopHeader = loopHeader;
    this.loopEnd = loopEnd;
    this.predecessors = predecessors;
    this.successors = successors;
    this.instructions = new Array<SequenceBlockInstruction>();
    this.phis = new Array<SequenceBlockPhi>();
  }
}

export class SequenceBlockInstruction {
  id: number;
  opcode: string;
  flags: string;
  inputs: Array<SequenceBlockOperand>;
  outputs: Array<SequenceBlockOperand>;
  gaps: Array<Array<[SequenceBlockOperand, SequenceBlockOperand]>>;
  temps: Array<any>;

  constructor(id: number, opcode: string, flags: string, temps: Array<any>) {
    this.id = id;
    this.opcode = opcode;
    this.flags = flags;
    this.inputs = new Array<SequenceBlockOperand>();
    this.outputs = new Array<SequenceBlockOperand>();
    this.gaps = new Array<Array<[SequenceBlockOperand, SequenceBlockOperand]>>();
    this.temps = temps;
  }
}

export class SequenceBlockPhi {
  operands: Array<string>;
  output: SequenceBlockOperand;

  constructor(operands: Array<string>, output: SequenceBlockOperand) {
    this.operands = operands;
    this.output = output;
  }
}

export class RegisterAllocation {
  fixedDoubleLiveRanges: Array<Range>;
  fixedLiveRanges: Array<Range>;
  liveRanges: Array<Range>;

  constructor() {
    this.fixedDoubleLiveRanges = new Array<Range>();
    this.fixedLiveRanges = new Array<Range>();
    this.liveRanges = new Array<Range>();
  }
}

export class Range {
  isDeferred: boolean;
  instructionRange: [number, number];
  childRanges: Array<ChildRange>;

  constructor(isDeferred: boolean, instructionRange: [number, number]) {
    this.isDeferred = isDeferred;
    this.instructionRange = instructionRange;
    this.childRanges = new Array<ChildRange>();
  }

  public fixedRegisterName(): string {
    const operation = this.childRanges[0].op;
    if (operation instanceof SequenceBlockOperand) {
      return operation.text;
    }
    return operation;
  }
}

export class ChildRange {
  id: string;
  type: string;
  op: SequenceBlockOperand | string;
  intervals: Array<[number, number]>;
  uses: Array<number>;

  constructor(id: string, type: string, op: SequenceBlockOperand | string,
              intervals: Array<[number, number]>, uses: Array<number>) {
    this.id = id;
    this.type = type;
    this.op = op;
    this.intervals = intervals;
    this.uses = uses;
  }

  public getTooltip(registerIndex: number): RangeToolTip {
    switch (this.type) {
      case "none":
        return new RangeToolTip(C.INTERVAL_TEXT_FOR_NONE, false);
      case "spill_range":
        return new RangeToolTip(`${C.INTERVAL_TEXT_FOR_STACK}${registerIndex}`, true);
      default:
        if (this.op instanceof SequenceBlockOperand && this.op.type == "constant") {
          new RangeToolTip(C.INTERVAL_TEXT_FOR_CONST, false);
        } else {
          if (this.op instanceof SequenceBlockOperand && this.op.text) {
            new RangeToolTip(this.op.text, true);
          } else if (typeof this.op === "string") {
            new RangeToolTip(this.op, true);
          }
        }
    }
    return new RangeToolTip("", false);
  }

  public isFloatingPoint(): boolean {
    return this.op instanceof SequenceBlockOperand && this.op.tooltip.includes("Float");
  }
}

export class RangeToolTip {
  text: string;
  isId: boolean;

  constructor(text: string, isId: boolean) {
    this.text = text;
    this.isId = isId;
  }
}

export class SequenceBlockOperand {
  type: string;
  text: string;
  tooltip: string;

  constructor(type: string, text: string, tooltip: string) {
    this.type = type;
    this.text = text;
    this.tooltip = tooltip;
  }
}
