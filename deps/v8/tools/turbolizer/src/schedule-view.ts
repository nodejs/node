// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Schedule, SourceResolver } from "../src/source-resolver";
import { TextView } from "../src/text-view";

export class ScheduleView extends TextView {
  schedule: Schedule;
  sourceResolver: SourceResolver;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "schedule");
    return pane;
  }

  constructor(parentId, broker) {
    super(parentId, broker);
    this.sourceResolver = broker.sourceResolver;
  }

  attachSelection(s) {
    const view = this;
    if (!(s instanceof Set)) return;
    view.selectionHandler.clear();
    view.blockSelectionHandler.clear();
    const selected = new Array();
    for (const key of s) selected.push(key);
    view.selectionHandler.select(selected, true);
  }

  detachSelection() {
    this.blockSelection.clear();
    return this.selection.detachSelection();
  }

  initializeContent(data, rememberedSelection) {
    this.divNode.innerHTML = '';
    this.schedule = data.schedule;
    this.addBlocks(data.schedule.blocks);
    this.attachSelection(rememberedSelection);
    this.show();
  }

  createElementFromString(htmlString) {
    const div = document.createElement('div');
    div.innerHTML = htmlString.trim();
    return div.firstChild;
  }

  elementForBlock(block) {
    const view = this;
    function createElement(tag: string, cls: string, content?: string) {
      const el = document.createElement(tag);
      el.className = cls;
      if (content != undefined) el.innerHTML = content;
      return el;
    }

    function mkNodeLinkHandler(nodeId) {
      return function (e) {
        e.stopPropagation();
        if (!e.shiftKey) {
          view.selectionHandler.clear();
        }
        view.selectionHandler.select([nodeId], true);
      };
    }

    function getMarker(start, end) {
      if (start != end) {
        return ["&#8857;", `This node generated instructions in range [${start},${end}). ` +
          `This is currently unreliable for constants.`];
      }
      if (start != -1) {
        return ["&#183;", `The instruction selector did not generate instructions ` +
          `for this node, but processed the node at instruction ${start}. ` +
          `This usually means that this node was folded into another node; ` +
          `the highlighted machine code is a guess.`];
      }
      return ["", `This not is not in the final schedule.`];
    }

    function createElementForNode(node) {
      const nodeEl = createElement("div", "node");

      const [start, end] = view.sourceResolver.getInstruction(node.id);
      const [marker, tooltip] = getMarker(start, end);
      const instrMarker = createElement("div", "instr-marker com", marker);
      instrMarker.setAttribute("title", tooltip);
      instrMarker.onclick = mkNodeLinkHandler(node.id);
      nodeEl.appendChild(instrMarker);

      const nodeId = createElement("div", "node-id tag clickable", node.id);
      nodeId.onclick = mkNodeLinkHandler(node.id);
      view.addHtmlElementForNodeId(node.id, nodeId);
      nodeEl.appendChild(nodeId);
      const nodeLabel = createElement("div", "node-label", node.label);
      nodeEl.appendChild(nodeLabel);
      if (node.inputs.length > 0) {
        const nodeParameters = createElement("div", "parameter-list comma-sep-list");
        for (const param of node.inputs) {
          const paramEl = createElement("div", "parameter tag clickable", param);
          nodeParameters.appendChild(paramEl);
          paramEl.onclick = mkNodeLinkHandler(param);
          view.addHtmlElementForNodeId(param, paramEl);
        }
        nodeEl.appendChild(nodeParameters);
      }

      return nodeEl;
    }

    function mkBlockLinkHandler(blockId) {
      return function (e) {
        e.stopPropagation();
        if (!e.shiftKey) {
          view.blockSelectionHandler.clear();
        }
        view.blockSelectionHandler.select(["" + blockId], true);
      };
    }

    const scheduleBlock = createElement("div", "schedule-block");
    scheduleBlock.classList.toggle("deferred", block.isDeferred);

    const [start, end] = view.sourceResolver.getInstructionRangeForBlock(block.id);
    const instrMarker = createElement("div", "instr-marker com", "&#8857;");
    instrMarker.setAttribute("title", `Instructions range for this block is [${start}, ${end})`);
    instrMarker.onclick = mkBlockLinkHandler(block.id);
    scheduleBlock.appendChild(instrMarker);

    const blockId = createElement("div", "block-id com clickable", block.id);
    blockId.onclick = mkBlockLinkHandler(block.id);
    scheduleBlock.appendChild(blockId);
    const blockPred = createElement("div", "predecessor-list block-list comma-sep-list");
    for (const pred of block.pred) {
      const predEl = createElement("div", "block-id com clickable", pred);
      predEl.onclick = mkBlockLinkHandler(pred);
      blockPred.appendChild(predEl);
    }
    if (block.pred.length) scheduleBlock.appendChild(blockPred);
    const nodes = createElement("div", "nodes");
    for (const node of block.nodes) {
      nodes.appendChild(createElementForNode(node));
    }
    scheduleBlock.appendChild(nodes);
    const blockSucc = createElement("div", "successor-list block-list comma-sep-list");
    for (const succ of block.succ) {
      const succEl = createElement("div", "block-id com clickable", succ);
      succEl.onclick = mkBlockLinkHandler(succ);
      blockSucc.appendChild(succEl);
    }
    if (block.succ.length) scheduleBlock.appendChild(blockSucc);
    this.addHtmlElementForBlockId(block.id, scheduleBlock);
    return scheduleBlock;
  }

  addBlocks(blocks) {
    for (const block of blocks) {
      const blockEl = this.elementForBlock(block);
      this.divNode.appendChild(blockEl);
    }
  }

  lineString(node) {
    return `${node.id}: ${node.label}(${node.inputs.join(", ")})`;
  }

  searchInputAction(searchBar, e, onlyVisible) {
    e.stopPropagation();
    this.selectionHandler.clear();
    const query = searchBar.value;
    if (query.length == 0) return;
    const select = [];
    window.sessionStorage.setItem("lastSearch", query);
    const reg = new RegExp(query);
    for (const node of this.schedule.nodes) {
      if (node === undefined) continue;
      if (reg.exec(this.lineString(node)) != null) {
        select.push(node.id);
      }
    }
    this.selectionHandler.select(select, true);
  }
}
