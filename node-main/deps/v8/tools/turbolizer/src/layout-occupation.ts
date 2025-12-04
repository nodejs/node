// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { Graph } from "./graph";
import { GraphNode } from "./phases/graph-phase/graph-node";
import { TurboshaftGraph } from "./turboshaft-graph";
import { TurboshaftGraphBlock } from "./phases/turboshaft-graph-phase/turboshaft-graph-block";

export class LayoutOccupation {
  graph: Graph | TurboshaftGraph;
  filledSlots: Array<boolean>;
  occupations: Array<[number, number]>;
  minSlot: number;
  maxSlot: number;

  constructor(graph: Graph | TurboshaftGraph) {
    this.graph = graph;
    this.filledSlots = new Array<boolean>();
    this.occupations = new Array<[number, number]>();
    this.minSlot = 0;
    this.maxSlot = 0;
  }

  public clearOutputs(source: GraphNode | TurboshaftGraphBlock, extendHeight: boolean): void {
    for (const edge of source.outputs) {
      if (!edge.isVisible()) continue;
      for (const inputEdge of edge.target.inputs) {
        if (inputEdge.source === source) {
          const horizontalPos = edge.getInputHorizontalPosition(this.graph, extendHeight);
          this.clearPositionRangeWithMargin(horizontalPos, horizontalPos, C.NODE_INPUT_WIDTH / 2);
        }
      }
    }
  }

  public clearOccupied(): void {
    for (const [firstSlot, endSlotExclusive] of this.occupations) {
      this.clearSlotRange(firstSlot, endSlotExclusive);
    }
    this.occupations = new Array<[number, number]>();
  }

  public occupy(item: GraphNode | TurboshaftGraphBlock): number {
    const width = item.getWidth();
    const margin = C.MINIMUM_EDGE_SEPARATION;
    const paddedWidth = width + 2 * margin;
    const [direction, position] = this.getPlacementHint(item);
    const x = position - paddedWidth + margin;
    this.trace(`${item.id} placement hint [${x}, ${(x + paddedWidth)})`);
    const placement = this.findSpace(x, paddedWidth, direction);
    const [firstSlot, slotWidth] = placement;
    const endSlotExclusive = firstSlot + slotWidth - 1;
    this.occupySlotRange(firstSlot, endSlotExclusive);
    this.occupations.push([firstSlot, endSlotExclusive]);
    if (direction < 0) {
      return this.slotToLeftPosition(firstSlot + slotWidth) - width - margin;
    } else if (direction > 0) {
      return this.slotToLeftPosition(firstSlot) + margin;
    } else {
      return this.slotToLeftPosition(firstSlot + slotWidth / 2) - (width / 2);
    }
  }

  public occupyInputs(item: GraphNode | TurboshaftGraphBlock, extendHeight: boolean): void {
    for (let i = 0; i < item.inputs.length; ++i) {
      if (item.inputs[i].isVisible()) {
        const edge = item.inputs[i];
        if (!edge.isBackEdge()) {
          const horizontalPos = edge.getInputHorizontalPosition(this.graph, extendHeight);
          this.trace(`Occupying input ${i} of ${item.id} at ${horizontalPos}`);
          this.occupyPositionRangeWithMargin(horizontalPos, horizontalPos, C.NODE_INPUT_WIDTH / 2);
        }
      }
    }
  }

  public print(): void {
    let output = "";
    for (let currentSlot = -40; currentSlot < 40; ++currentSlot) {
      if (currentSlot != 0) {
        output += " ";
      } else {
        output += "|";
      }
    }
    console.log(output);
    output = "";
    for (let currentSlot2 = -40; currentSlot2 < 40; ++currentSlot2) {
      if (this.filledSlots[this.slotToIndex(currentSlot2)]) {
        output += "*";
      } else {
        output += " ";
      }
    }
    console.log(output);
  }

  private getPlacementHint(item: GraphNode | TurboshaftGraphBlock): [number, number] {
    let position = 0;
    let direction = -1;
    let outputEdges = 0;
    let inputEdges = 0;
    for (const outputEdge of item.outputs) {
      if (!outputEdge.isVisible()) continue;
      const output = outputEdge.target;
      for (let l = 0; l < output.inputs.length; ++l) {
        if (output.rank > item.rank) {
          const inputEdge = output.inputs[l];
          if (inputEdge.isVisible()) ++inputEdges;
          if (output.inputs[l].source == item) {
            position += output.x + output.getInputX(l) + C.NODE_INPUT_WIDTH / 2;
            outputEdges++;
            if (l >= (output.inputs.length / 2)) {
              direction = 1;
            }
          }
        }
      }
    }
    if (outputEdges != 0) {
      position /= outputEdges;
    }
    if (outputEdges > 1 || inputEdges == 1) {
      direction = 0;
    }
    return [direction, position];
  }

  private occupyPositionRange(from: number, to: number): void {
    this.occupySlotRange(this.positionToSlot(from), this.positionToSlot(to - 1));
  }

  private clearPositionRange(from: number, to: number): void {
    this.clearSlotRange(this.positionToSlot(from), this.positionToSlot(to - 1));
  }

  private occupySlotRange(from: number, to: number): void {
    this.trace(`Occupied [${this.slotToLeftPosition(from)} ${this.slotToLeftPosition(to + 1)})`);
    this.setIndexRange(from, to, true);
  }

  private clearSlotRange(from: number, to: number): void {
    this.trace(`Cleared [${this.slotToLeftPosition(from)} ${this.slotToLeftPosition(to + 1)})`);
    this.setIndexRange(from, to, false);
  }

  private clearPositionRangeWithMargin(from: number, to: number, margin: number): void {
    const fromMargin = from - Math.floor(margin);
    const toMargin = to + Math.floor(margin);
    this.clearPositionRange(fromMargin, toMargin);
  }

  private occupyPositionRangeWithMargin(from: number, to: number, margin: number): void {
    const fromMargin = from - Math.floor(margin);
    const toMargin = to + Math.floor(margin);
    this.occupyPositionRange(fromMargin, toMargin);
  }

  private findSpace(pos: number, width: number, direction: number): [number, number] {
    const widthSlots = Math.floor((width + C.NODE_INPUT_WIDTH - 1) /
      C.NODE_INPUT_WIDTH);

    const currentSlot = this.positionToSlot(pos + width / 2);
    let widthSlotsRemainingLeft = widthSlots;
    let widthSlotsRemainingRight = widthSlots;
    let slotsChecked = 0;

    while (true) {
      const mod = slotsChecked++ % 2;
      const currentScanSlot = currentSlot + (mod ? -1 : 1) * (slotsChecked >> 1);
      if (!this.filledSlots[this.slotToIndex(currentScanSlot)]) {
        if (mod) {
          if (direction <= 0) --widthSlotsRemainingLeft;
        } else if (direction >= 0) {
          --widthSlotsRemainingRight;
        }
        if (widthSlotsRemainingLeft == 0 || widthSlotsRemainingRight == 0 ||
          (widthSlotsRemainingLeft + widthSlotsRemainingRight) == widthSlots &&
          (widthSlots == slotsChecked)) {
          return mod ? [currentScanSlot, widthSlots]
            : [currentScanSlot - widthSlots + 1, widthSlots];
        }
      } else {
        if (mod) {
          widthSlotsRemainingLeft = widthSlots;
        } else {
          widthSlotsRemainingRight = widthSlots;
        }
      }
    }
  }

  private setIndexRange(from: number, to: number, value: boolean): void {
    if (to < from) throw ("Illegal slot range");
    while (from <= to) {
      this.maxSlot = Math.max(from, this.maxSlot);
      this.minSlot = Math.min(from, this.minSlot);
      this.filledSlots[this.slotToIndex(from++)] = value;
    }
  }

  private positionToSlot(position: number): number {
    return Math.floor(position / C.NODE_INPUT_WIDTH);
  }

  private slotToIndex(slot: number): number {
    return slot >= 0 ? slot * 2 : slot * 2 + 1;
  }

  private slotToLeftPosition(slot: number): number {
    return slot * C.NODE_INPUT_WIDTH;
  }

  private trace(message): void {
    if (C.TRACE_LAYOUT) {
      console.log(message);
    }
  }
}
