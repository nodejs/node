// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./../common/constants";
import { storageSetItem } from "../common/util";
import { TextView } from "./text-view";
import { SelectionStorage } from "../selection/selection-storage";
import { SelectionBroker } from "../selection/selection-broker";
import { ScheduleBlock, ScheduleNode, SchedulePhase } from "../phases/schedule-phase";

export class ScheduleView extends TextView {
  schedule: SchedulePhase;

  constructor(parent: HTMLElement, broker: SelectionBroker) {
    super(parent, broker);
    this.sourceResolver = broker.sourceResolver;
  }

  public createViewElement(): HTMLDivElement {
    const pane = document.createElement("div");
    pane.setAttribute("id", C.SCHEDULE_PANE_ID);
    pane.classList.add("scrollable");
    pane.setAttribute("tabindex", "0");
    return pane;
  }

  public initializeContent(schedule: SchedulePhase, rememberedSelection: SelectionStorage): void {
    this.divNode.innerHTML = "";
    this.schedule = schedule;
    this.clearSelectionMaps();
    this.addBlocks(schedule.data.blocksRpo);
    this.show();
    if (rememberedSelection) {
      const adaptedSelection = this.adaptSelection(rememberedSelection);
      this.attachSelection(adaptedSelection);
    }
  }

  public detachSelection(): SelectionStorage {
    return new SelectionStorage(this.nodeSelections.current.detachSelection(),
      this.blockSelections.current.detachSelection());
  }

  public adaptSelection(selection: SelectionStorage): SelectionStorage {
    for (const key of selection.nodes.keys()) selection.adaptedNodes.add(key);
    for (const key of selection.blocks.keys()) selection.adaptedBocks.add(key);
    return selection;
  }

  public searchInputAction(searchBar: HTMLInputElement, e: KeyboardEvent, onlyVisible: boolean):
    void {
    e.stopPropagation();
    this.nodeSelectionHandler.clear();
    const query = searchBar.value;
    if (query.length == 0) return;
    const select = new Array<number>();
    storageSetItem("lastSearch", query);
    const reg = new RegExp(query);
    for (const node of this.schedule.data.nodes) {
      if (node === undefined) continue;
      if (reg.exec(node.toString()) !== null) {
        select.push(node.id);
      }
    }
    this.nodeSelectionHandler.select(select, true, false);
  }

  private addBlocks(blocks: Array<ScheduleBlock>) {
    for (const block of blocks) {
      const blockEl = this.createElementForBlock(block);
      this.divNode.appendChild(blockEl);
    }
  }

  private attachSelection(adaptedSelection: SelectionStorage): void {
    if (!(adaptedSelection instanceof SelectionStorage)) return;
    this.blockSelectionHandler.clear();
    this.nodeSelectionHandler.clear();
    this.blockSelectionHandler.select(
                Array.from(adaptedSelection.adaptedBocks).map(block => Number(block)), true, true);
    this.nodeSelectionHandler.select(adaptedSelection.adaptedNodes, true, true);
  }

  private createElementForBlock(block: ScheduleBlock): HTMLElement {
    const scheduleBlock = this.createElement("div", "schedule-block");
    scheduleBlock.classList.toggle("deferred", block.deferred);

    const [start, end] = this.sourceResolver.instructionsPhase
      .getInstructionRangeForBlock(block.rpo);
    const instrMarker = this.createElement("div", "instr-marker com", "&#8857;");
    instrMarker.setAttribute("title", `Instructions range for this block is [${start}, ${end})`);
    instrMarker.onclick = this.mkBlockLinkHandler(block.rpo);
    scheduleBlock.appendChild(instrMarker);

    let displayStr = String(block.rpo) + " Id:" + String(block.id);
    if(block.count != -1) displayStr += " Count:" + String(block.count);
    const blocksRpoId = this.createElement("div", "block-id com clickable", displayStr);
    blocksRpoId.onclick = this.mkBlockLinkHandler(block.rpo);
    scheduleBlock.appendChild(blocksRpoId);
    const blockPred = this.createElement("div", "predecessor-list block-list comma-sep-list");
    for (const pred of block.predecessors) {
      const predEl = this.createElement("div", "block-id com clickable", String(pred));
      predEl.onclick = this.mkBlockLinkHandler(pred);
      blockPred.appendChild(predEl);
    }
    if (block.predecessors.length) scheduleBlock.appendChild(blockPred);
    const nodes = this.createElement("div", "nodes");
    for (const node of block.nodes) {
      nodes.appendChild(this.createElementForNode(node));
    }
    scheduleBlock.appendChild(nodes);

    const blockSucc = this.createElement("div", "successor-list block-list comma-sep-list");
    for (const succ of block.successors) {
      const succEl = this.createElement("div", "block-id com clickable", String(succ));
      succEl.onclick = this.mkBlockLinkHandler(succ);
      blockSucc.appendChild(succEl);
    }
    if (block.successors.length) scheduleBlock.appendChild(blockSucc);
    this.addHtmlElementForBlockId(block.rpo, scheduleBlock);
    return scheduleBlock;
  }

  private createElementForNode(node: ScheduleNode): HTMLElement {
    const nodeEl = this.createElement("div", "node");

    const [start, end] = this.sourceResolver.instructionsPhase.getInstruction(node.id);
    const [marker, tooltip] = this.sourceResolver.instructionsPhase
      .getInstructionMarker(start, end);
    const instrMarker = this.createElement("div", "instr-marker com", marker);
    instrMarker.setAttribute("title", tooltip);
    instrMarker.onclick = this.mkNodeLinkHandler(node.id);
    nodeEl.appendChild(instrMarker);

    const nodeIdEl = this.createElement("div", "node-id tag clickable", String(node.id));
    nodeIdEl.onclick = this.mkNodeLinkHandler(node.id);
    this.addHtmlElementForNodeId(node.id, nodeIdEl);
    nodeEl.appendChild(nodeIdEl);
    const nodeLabel = this.createElement("div", "node-label", node.label);
    nodeEl.appendChild(nodeLabel);
    if (node.inputs.length > 0) {
      const nodeParameters = this.createElement("div", "parameter-list comma-sep-list");
      for (const param of node.inputs) {
        const paramEl = this.createElement("div", "parameter tag clickable", String(param));
        nodeParameters.appendChild(paramEl);
        paramEl.onclick = this.mkNodeLinkHandler(param);
        this.addHtmlElementForNodeId(param, paramEl);
      }
      nodeEl.appendChild(nodeParameters);
    }

    return nodeEl;
  }

  private mkBlockLinkHandler(blockId: number): (e: MouseEvent) => void {
    const view = this;
    return function (e: MouseEvent) {
      e.stopPropagation();
      if (!e.shiftKey) {
        view.blockSelectionHandler.clear();
      }
      view.blockSelectionHandler.select([blockId], true, false);
    };
  }

  private mkNodeLinkHandler(nodeId: number): (e: MouseEvent) => void {
    const view = this;
    return function (e: MouseEvent) {
      e.stopPropagation();
      if (!e.shiftKey) {
        view.nodeSelectionHandler.clear();
      }
      view.nodeSelectionHandler.select([nodeId], true, false);
    };
  }

  private createElement(tag: string, cls: string, content?: string) {
    const el = document.createElement(tag);
    el.className = cls;
    if (content !== undefined) el.innerHTML = content;
    return el;
  }
}
