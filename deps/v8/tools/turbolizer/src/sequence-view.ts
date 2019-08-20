// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Sequence } from "../src/source-resolver";
import { isIterable } from "../src/util";
import { TextView } from "../src/text-view";

export class SequenceView extends TextView {
  sequence: Sequence;
  searchInfo: Array<any>;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "sequence");
    return pane;
  }

  constructor(parentId, broker) {
    super(parentId, broker);
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
    this.searchInfo = [];
    this.divNode.addEventListener('click', (e: MouseEvent) => {
      if (!(e.target instanceof HTMLElement)) return;
      const instructionId = Number.parseInt(e.target.dataset.instructionId, 10);
      if (!instructionId) return;
      if (!e.shiftKey) this.broker.broadcastClear(null);
      this.broker.broadcastInstructionSelect(null, [instructionId], true);
    });
    this.addBlocks(this.sequence.blocks);
    this.attachSelection(rememberedSelection);
    this.show();
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

    function elementForOperand(operand, searchInfo) {
      const text = operand.text;
      const operandEl = createElement("div", ["parameter", "tag", "clickable", operand.type], text);
      if (operand.tooltip) {
        operandEl.setAttribute("title", operand.tooltip);
      }
      operandEl.onclick = mkOperandLinkHandler(text);
      searchInfo.push(text);
      view.addHtmlElementForNodeId(text, operandEl);
      return operandEl;
    }

    function elementForInstruction(instruction, searchInfo) {
      const instNodeEl = createElement("div", "instruction-node");

      const instId = createElement("div", "instruction-id", instruction.id);
      instId.classList.add("clickable");
      instId.dataset.instructionId = instruction.id;
      instNodeEl.appendChild(instId);

      const instContentsEl = createElement("div", "instruction-contents");
      instNodeEl.appendChild(instContentsEl);

      // Print gap moves.
      const gapEl = createElement("div", "gap", "gap");
      let hasGaps = false;
      for (const gap of instruction.gaps) {
        const moves = createElement("div", ["comma-sep-list", "gap-move"]);
        for (const move of gap) {
          hasGaps = true;
          const moveEl = createElement("div", "move");
          const destinationEl = elementForOperand(move[0], searchInfo);
          moveEl.appendChild(destinationEl);
          const assignEl = createElement("div", "assign", "=");
          moveEl.appendChild(assignEl);
          const sourceEl = elementForOperand(move[1], searchInfo);
          moveEl.appendChild(sourceEl);
          moves.appendChild(moveEl);
        }
        gapEl.appendChild(moves);
      }
      if (hasGaps) {
        instContentsEl.appendChild(gapEl);
      }

      const instEl = createElement("div", "instruction");
      instContentsEl.appendChild(instEl);

      if (instruction.outputs.length > 0) {
        const outputs = createElement("div", ["comma-sep-list", "input-output-list"]);
        for (const output of instruction.outputs) {
          const outputEl = elementForOperand(output, searchInfo);
          outputs.appendChild(outputEl);
        }
        instEl.appendChild(outputs);
        const assignEl = createElement("div", "assign", "=");
        instEl.appendChild(assignEl);
      }

      let text = instruction.opcode + instruction.flags;
      const instLabel = createElement("div", "node-label", text)
      if (instruction.opcode == "ArchNop" && instruction.outputs.length == 1 && instruction.outputs[0].tooltip) {
        instLabel.innerText = instruction.outputs[0].tooltip;
      }

      searchInfo.push(text);
      view.addHtmlElementForNodeId(text, instLabel);
      instEl.appendChild(instLabel);

      if (instruction.inputs.length > 0) {
        const inputs = createElement("div", ["comma-sep-list", "input-output-list"]);
        for (const input of instruction.inputs) {
          const inputEl = elementForOperand(input, searchInfo);
          inputs.appendChild(inputEl);
        }
        instEl.appendChild(inputs);
      }

      if (instruction.temps.length > 0) {
        const temps = createElement("div", ["comma-sep-list", "input-output-list", "temps"]);
        for (const temp of instruction.temps) {
          const tempEl = elementForOperand(temp, searchInfo);
          temps.appendChild(tempEl);
        }
        instEl.appendChild(temps);
      }

      return instNodeEl;
    }

    const sequenceBlock = createElement("div", "schedule-block");
    sequenceBlock.classList.toggle("deferred", block.deferred);

    const blockId = createElement("div", ["block-id", "com", "clickable"], block.id);
    blockId.onclick = mkBlockLinkHandler(block.id);
    sequenceBlock.appendChild(blockId);
    const blockPred = createElement("div", ["predecessor-list", "block-list", "comma-sep-list"]);
    for (const pred of block.predecessors) {
      const predEl = createElement("div", ["block-id", "com", "clickable"], pred);
      predEl.onclick = mkBlockLinkHandler(pred);
      blockPred.appendChild(predEl);
    }
    if (block.predecessors.length > 0) sequenceBlock.appendChild(blockPred);
    const phis = createElement("div", "phis");
    sequenceBlock.appendChild(phis);

    const phiLabel = createElement("div", "phi-label", "phi:");
    phis.appendChild(phiLabel);

    const phiContents = createElement("div", "phi-contents");
    phis.appendChild(phiContents);

    for (const phi of block.phis) {
      const phiEl = createElement("div", "phi");
      phiContents.appendChild(phiEl);

      const outputEl = elementForOperand(phi.output, this.searchInfo);
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
      instructions.appendChild(elementForInstruction(instruction, this.searchInfo));
    }
    sequenceBlock.appendChild(instructions);
    const blockSucc = createElement("div", ["successor-list", "block-list", "comma-sep-list"]);
    for (const succ of block.successors) {
      const succEl = createElement("div", ["block-id", "com", "clickable"], succ);
      succEl.onclick = mkBlockLinkHandler(succ);
      blockSucc.appendChild(succEl);
    }
    if (block.successors.length > 0) sequenceBlock.appendChild(blockSucc);
    this.addHtmlElementForBlockId(block.id, sequenceBlock);
    return sequenceBlock;
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
    for (const item of this.searchInfo) {
      if (reg.exec(item) != null) {
        select.push(item);
      }
    }
    this.selectionHandler.select(select, true);
  }
}
