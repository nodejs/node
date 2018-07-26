// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class ScheduleView extends TextView {

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "schedule");
    pane.innerHTML =
    `<pre id='schedule-text-pre' class='prettyprint prettyprinted'>
       <ul id='schedule-list' class='nolinenums noindent'>
       </ul>
     </pre>`;
    return pane;
  }

  constructor(parentId, broker) {
    super(parentId, broker, null, false);
  }

  attachSelection(s) {
    const view = this;
    if (!(s instanceof Set)) return;
    view.selectionHandler.clear();
    view.blockSelectionHandler.clear();
    view.sourcePositionSelectionHandler.clear();
    const selected = new Array();
    for (const key of s) selected.push(key);
    view.selectionHandler.select(selected, true);
  }

  createElementFromString(htmlString) {
    var div = document.createElement('div');
    div.innerHTML = htmlString.trim();
    return div.firstChild;
  }


  elementForBlock(block) {
    const view = this;
    function createElement(tag, cls, content) {
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

    function createElementForNode(node) {
      const nodeEl = createElement("div", "node");
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
      nodes.appendChild(createElementForNode(node, block.id));
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

  initializeContent(data, rememberedSelection) {
    this.clearText();
    this.schedule = data.schedule
    this.addBlocks(data.schedule.blocks);
    this.attachSelection(rememberedSelection);
  }

  detachSelection() {
    this.blockSelection.clear();
    this.sourcePositionSelection.clear();
    return this.selection.detachSelection();
  }

  lineString(node) {
    return `${node.id}: ${node.label}(${node.inputs.join(", ")})`
  }

  searchInputAction(view, searchBar) {
    d3.event.stopPropagation();
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
}
