// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Schedule,SourceResolver} from "./source-resolver.js"
import {isIterable} from "./util.js"
import {PhaseView} from "./view.js"
import {TextView} from "./text-view.js"

export class ScheduleView extends TextView implements PhaseView {
  schedule: Schedule;
  sourceResolver: SourceResolver;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "schedule");
    return pane;
  }

  constructor(parentId, broker) {
    super(parentId, broker, null);
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
    this.schedule = data.schedule
    this.addBlocks(data.schedule.blocks);
    this.attachSelection(rememberedSelection);
  }

  createElementFromString(htmlString) {
    var div = document.createElement('div');
    div.innerHTML = htmlString.trim();
    return div.firstChild;
  }

  elementForBlock(block) {
    const view = this;
    function createElement(tag: string, cls: string | Array<string>, content?: string) {
      const el = document.createElement(tag);
      if (isIterable(cls)) {
        for (const c of cls) el.classList.add(c);
      } else {
        el.classList.add(cls);
      }
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
      return ["", `This not is not in the final schedule.`]
    }

    function createElementForNode(node) {
      const nodeEl = createElement("div", "node");

      const [start, end] = view.sourceResolver.getInstruction(node.id);
      const [marker, tooltip] = getMarker(start, end);
      const instrMarker = createElement("div", ["instr-marker", "com"], marker);
      instrMarker.setAttribute("title", tooltip);
      instrMarker.onclick = mkNodeLinkHandler(node.id);
      nodeEl.appendChild(instrMarker);


      const node_id = createElement("div", ["node-id", "tag", "clickable"], node.id);
      node_id.onclick = mkNodeLinkHandler(node.id);
      view.addHtmlElementForNodeId(node.id, node_id);
      nodeEl.appendChild(node_id);
      const node_label = createElement("div", "node-label", node.label);
      nodeEl.appendChild(node_label);
      if (node.inputs.length > 0) {
        const node_parameters = createElement("div", ["parameter-list", "comma-sep-list"]);
        for (const param of node.inputs) {
          const paramEl = createElement("div", ["parameter", "tag", "clickable"], param);
          node_parameters.appendChild(paramEl);
          paramEl.onclick = mkNodeLinkHandler(param);
          view.addHtmlElementForNodeId(param, paramEl);
        }
        nodeEl.appendChild(node_parameters);
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

    const schedule_block = createElement("div", "schedule-block");

    const [start, end] = view.sourceResolver.getInstructionRangeForBlock(block.id);
    const instrMarker = createElement("div", ["instr-marker", "com"], "&#8857;");
    instrMarker.setAttribute("title", `Instructions range for this block is [${start}, ${end})`)
    instrMarker.onclick = mkBlockLinkHandler(block.id);
    schedule_block.appendChild(instrMarker);

    const block_id = createElement("div", ["block-id", "com", "clickable"], block.id);
    block_id.onclick = mkBlockLinkHandler(block.id);
    schedule_block.appendChild(block_id);
    const block_pred = createElement("div", ["predecessor-list", "block-list", "comma-sep-list"]);
    for (const pred of block.pred) {
      const predEl = createElement("div", ["block-id", "com", "clickable"], pred);
      predEl.onclick = mkBlockLinkHandler(pred);
      block_pred.appendChild(predEl);
    }
    if (block.pred.length) schedule_block.appendChild(block_pred);
    const nodes = createElement("div", "nodes");
    for (const node of block.nodes) {
      nodes.appendChild(createElementForNode(node));
    }
    schedule_block.appendChild(nodes);
    const block_succ = createElement("div", ["successor-list", "block-list", "comma-sep-list"]);
    for (const succ of block.succ) {
      const succEl = createElement("div", ["block-id", "com", "clickable"], succ);
      succEl.onclick = mkBlockLinkHandler(succ);
      block_succ.appendChild(succEl);
    }
    if (block.succ.length) schedule_block.appendChild(block_succ);
    this.addHtmlElementForBlockId(block.id, schedule_block);
    return schedule_block;
  }

  addBlocks(blocks) {
    for (const block of blocks) {
      const blockEl = this.elementForBlock(block);
      this.divNode.appendChild(blockEl);
    }
  }

  lineString(node) {
    return `${node.id}: ${node.label}(${node.inputs.join(", ")})`
  }

  searchInputAction(searchBar, e) {
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
        select.push(node.id)
      }
    }
    this.selectionHandler.select(select, true);
  }

  onresize() { }
}
