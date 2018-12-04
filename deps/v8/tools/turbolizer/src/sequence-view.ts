// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Sequence} from "./source-resolver.js"
import {isIterable} from "./util.js"
import {PhaseView} from "./view.js"
import {TextView} from "./text-view.js"

export class SequenceView extends TextView implements PhaseView {
  sequence: Sequence;
  search_info: Array<any>;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "sequence");
    return pane;
  }

  constructor(parentId, broker) {
    super(parentId, broker, null);
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
    this.sequence = data.sequence;
    this.search_info = [];
    this.addBlocks(this.sequence.blocks);
    this.attachSelection(rememberedSelection);
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

    function mkLinkHandler(id, handler) {
      return function (e) {
        e.stopPropagation();
        if (!e.shiftKey) {
          handler.clear();
        }
        handler.select(["" + id], true);
      };
    }

    function mkBlockLinkHandler(blockId) {
      return mkLinkHandler(blockId, view.blockSelectionHandler);
    }

    function mkOperandLinkHandler(text) {
      return mkLinkHandler(text, view.selectionHandler);
    }

    function elementForOperand(operand, search_info) {
      var text = operand.text;
      const operandEl = createElement("div", ["parameter", "tag", "clickable", operand.type], text);
      if (operand.tooltip) {
        operandEl.setAttribute("title", operand.tooltip);
      }
      operandEl.onclick = mkOperandLinkHandler(text);
      search_info.push(text);
      view.addHtmlElementForNodeId(text, operandEl);
      return operandEl;
    }

    function elementForInstruction(instruction, search_info) {
      const instNodeEl = createElement("div", "instruction-node");

      const inst_id = createElement("div", "instruction-id", instruction.id);
      instNodeEl.appendChild(inst_id);

      const instContentsEl = createElement("div", "instruction-contents");
      instNodeEl.appendChild(instContentsEl);

      // Print gap moves.
      const gapEl = createElement("div", "gap", "gap");
      instContentsEl.appendChild(gapEl);
      for (const gap of instruction.gaps) {
        const moves = createElement("div", ["comma-sep-list", "gap-move"]);
        for (const move of gap) {
          const moveEl = createElement("div", "move");
          const destinationEl = elementForOperand(move[0], search_info);
          moveEl.appendChild(destinationEl);
          const assignEl = createElement("div", "assign", "=");
          moveEl.appendChild(assignEl);
          const sourceEl = elementForOperand(move[1], search_info);
          moveEl.appendChild(sourceEl);
          moves.appendChild(moveEl);
        }
        gapEl.appendChild(moves);
      }

      const instEl = createElement("div", "instruction");
      instContentsEl.appendChild(instEl);

      if (instruction.outputs.length > 0) {
        const outputs = createElement("div", ["comma-sep-list", "input-output-list"]);
        for (const output of instruction.outputs) {
          const outputEl = elementForOperand(output, search_info);
          outputs.appendChild(outputEl);
        }
        instEl.appendChild(outputs);
        const assignEl = createElement("div", "assign", "=");
        instEl.appendChild(assignEl);
      }

      var text = instruction.opcode + instruction.flags;
      const inst_label = createElement("div", "node-label", text);
      search_info.push(text);
      view.addHtmlElementForNodeId(text, inst_label);
      instEl.appendChild(inst_label);

      if (instruction.inputs.length > 0) {
        const inputs = createElement("div", ["comma-sep-list", "input-output-list"]);
        for (const input of instruction.inputs) {
          const inputEl = elementForOperand(input, search_info);
          inputs.appendChild(inputEl);
        }
        instEl.appendChild(inputs);
      }

      if (instruction.temps.length > 0) {
        const temps = createElement("div", ["comma-sep-list", "input-output-list", "temps"]);
        for (const temp of instruction.temps) {
          const tempEl = elementForOperand(temp, search_info);
          temps.appendChild(tempEl);
        }
        instEl.appendChild(temps);
      }

      return instNodeEl;
    }

    const sequence_block = createElement("div", "schedule-block");

    const block_id = createElement("div", ["block-id", "com", "clickable"], block.id);
    block_id.onclick = mkBlockLinkHandler(block.id);
    sequence_block.appendChild(block_id);
    const block_pred = createElement("div", ["predecessor-list", "block-list", "comma-sep-list"]);
    for (const pred of block.predecessors) {
      const predEl = createElement("div", ["block-id", "com", "clickable"], pred);
      predEl.onclick = mkBlockLinkHandler(pred);
      block_pred.appendChild(predEl);
    }
    if (block.predecessors.length > 0) sequence_block.appendChild(block_pred);
    const phis = createElement("div", "phis");
    sequence_block.appendChild(phis);

    const phiLabel = createElement("div", "phi-label", "phi:");
    phis.appendChild(phiLabel);

    const phiContents = createElement("div", "phi-contents");
    phis.appendChild(phiContents);

    for (const phi of block.phis) {
      const phiEl = createElement("div", "phi");
      phiContents.appendChild(phiEl);

      const outputEl = elementForOperand(phi.output, this.search_info);
      phiEl.appendChild(outputEl);

      const assignEl = createElement("div", "assign", "=");
      phiEl.appendChild(assignEl);

      for (const input of phi.operands) {
        const inputEl = createElement("div", ["parameter", "tag", "clickable"], input);
        phiEl.appendChild(inputEl);
      }
    }

    const instructions = createElement("div", "instructions");
    for (const instruction of block.instructions) {
      instructions.appendChild(elementForInstruction(instruction, this.search_info));
    }
    sequence_block.appendChild(instructions);
    const block_succ = createElement("div", ["successor-list", "block-list", "comma-sep-list"]);
    for (const succ of block.successors) {
      const succEl = createElement("div", ["block-id", "com", "clickable"], succ);
      succEl.onclick = mkBlockLinkHandler(succ);
      block_succ.appendChild(succEl);
    }
    if (block.successors.length > 0) sequence_block.appendChild(block_succ);
    this.addHtmlElementForBlockId(block.id, sequence_block);
    return sequence_block;
  }

  addBlocks(blocks) {
    for (const block of blocks) {
      const blockEl = this.elementForBlock(block);
      this.divNode.appendChild(blockEl);
    }
  }

  searchInputAction(searchBar, e) {
    e.stopPropagation();
    this.selectionHandler.clear();
    const query = searchBar.value;
    if (query.length == 0) return;
    const select = [];
    window.sessionStorage.setItem("lastSearch", query);
    const reg = new RegExp(query);
    for (const item of this.search_info) {
      if (reg.exec(item) != null) {
        select.push(item);
      }
    }
    this.selectionHandler.select(select, true);
  }

  onresize() { }
}
