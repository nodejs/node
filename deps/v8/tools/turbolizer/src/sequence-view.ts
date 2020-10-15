// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Sequence } from "../src/source-resolver";
import { createElement } from "../src/util";
import { TextView } from "../src/text-view";
import { RangeView } from "../src/range-view";

export class SequenceView extends TextView {
  sequence: Sequence;
  searchInfo: Array<any>;
  phaseSelect: HTMLSelectElement;
  numInstructions: number;
  currentPhaseIndex: number;
  phaseIndexes: Set<number>;
  isShown: boolean;
  rangeView: RangeView;
  showRangeView: boolean;
  toggleRangeViewEl: HTMLElement;

  createViewElement() {
    const pane = document.createElement('div');
    pane.setAttribute('id', "sequence");
    pane.classList.add("scrollable");
    pane.setAttribute("tabindex", "0");
    return pane;
  }

  constructor(parentId, broker) {
    super(parentId, broker);
    this.numInstructions = 0;
    this.phaseIndexes = new Set<number>();
    this.isShown = false;
    this.showRangeView = false;
    this.rangeView = null;
    this.toggleRangeViewEl = this.elementForToggleRangeView();
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

  show() {
    this.currentPhaseIndex = this.phaseSelect.selectedIndex;
    if (!this.isShown) {
      this.isShown = true;
      this.phaseIndexes.add(this.currentPhaseIndex);
      this.container.appendChild(this.divNode);
      this.container.getElementsByClassName("graph-toolbox")[0].appendChild(this.toggleRangeViewEl);
    }
    if (this.showRangeView) this.rangeView.show();
  }

  hide() {
    // A single SequenceView object is used for two phases (i.e before and after
    // register allocation), tracking the indexes lets the redundant hides and
    // shows be avoided when switching between the two.
    this.currentPhaseIndex = this.phaseSelect.selectedIndex;
    if (!this.phaseIndexes.has(this.currentPhaseIndex)) {
      this.isShown = false;
      this.container.removeChild(this.divNode);
      this.container.getElementsByClassName("graph-toolbox")[0].removeChild(this.toggleRangeViewEl);
      if (this.showRangeView) this.rangeView.hide();
    }
  }

  onresize() {
    if (this.showRangeView) this.rangeView.onresize();
  }

  initializeContent(data, rememberedSelection) {
    this.divNode.innerHTML = '';
    this.sequence = data.sequence;
    this.searchInfo = [];
    this.divNode.onclick = (e: MouseEvent) => {
      if (!(e.target instanceof HTMLElement)) return;
      const instructionId = Number.parseInt(e.target.dataset.instructionId, 10);
      if (!instructionId) return;
      if (!e.shiftKey) this.broker.broadcastClear(null);
      this.broker.broadcastInstructionSelect(null, [instructionId], true);
    };
    this.phaseSelect = (document.getElementById('phase-select') as HTMLSelectElement);
    this.currentPhaseIndex = this.phaseSelect.selectedIndex;

    this.addBlocks(this.sequence.blocks);
    const lastBlock = this.sequence.blocks[this.sequence.blocks.length - 1];
    this.numInstructions = lastBlock.instructions[lastBlock.instructions.length - 1].id + 1;
    this.addRangeView();
    this.attachSelection(rememberedSelection);
    this.show();
  }

  elementForBlock(block) {
    const view = this;

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

    function elementForOperandWithSpan(span, text, searchInfo, isVirtual) {
      const selectionText = isVirtual ? "virt_" + text : text;
      span.onclick = mkOperandLinkHandler(selectionText);
      searchInfo.push(text);
      view.addHtmlElementForNodeId(selectionText, span);
      const container = createElement("div", "");
      container.appendChild(span);
      return container;
    }

    function elementForOperand(operand, searchInfo) {
      let isVirtual = false;
      let className = "parameter tag clickable " + operand.type;
      if (operand.text[0] == 'v' && !(operand.tooltip && operand.tooltip.includes("Float"))) {
        isVirtual = true;
        className += " virtual-reg";
      }
      const span = createElement("span", className, operand.text);
      if (operand.tooltip) {
        span.setAttribute("title", operand.tooltip);
      }
      return elementForOperandWithSpan(span, operand.text, searchInfo, isVirtual);
    }

    function elementForPhiOperand(text, searchInfo) {
      const span = createElement("span", "parameter tag clickable virtual-reg", text);
      return elementForOperandWithSpan(span, text, searchInfo, true);
    }

    function elementForInstruction(instruction, searchInfo) {
      const instNodeEl = createElement("div", "instruction-node");

      const instId = createElement("div", "instruction-id", instruction.id);
      const offsets = view.sourceResolver.instructionToPcOffsets(instruction.id);
      instId.classList.add("clickable");
      instId.dataset.instructionId = instruction.id;
      if (offsets) {
        instId.setAttribute("title", `This instruction generated gap code at pc-offset 0x${offsets.gap.toString(16)}, code at pc-offset 0x${offsets.arch.toString(16)}, condition handling at pc-offset 0x${offsets.condition.toString(16)}.`);
      }
      instNodeEl.appendChild(instId);

      const instContentsEl = createElement("div", "instruction-contents");
      instNodeEl.appendChild(instContentsEl);

      // Print gap moves.
      const gapEl = createElement("div", "gap", "gap");
      let hasGaps = false;
      for (const gap of instruction.gaps) {
        const moves = createElement("div", "comma-sep-list gap-move");
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
        const outputs = createElement("div", "comma-sep-list input-output-list");
        for (const output of instruction.outputs) {
          const outputEl = elementForOperand(output, searchInfo);
          outputs.appendChild(outputEl);
        }
        instEl.appendChild(outputs);
        const assignEl = createElement("div", "assign", "=");
        instEl.appendChild(assignEl);
      }

      const text = instruction.opcode + instruction.flags;
      const instLabel = createElement("div", "node-label", text);
      if (instruction.opcode == "ArchNop" && instruction.outputs.length == 1 && instruction.outputs[0].tooltip) {
        instLabel.innerText = instruction.outputs[0].tooltip;
      }

      searchInfo.push(text);
      view.addHtmlElementForNodeId(text, instLabel);
      instEl.appendChild(instLabel);

      if (instruction.inputs.length > 0) {
        const inputs = createElement("div", "comma-sep-list input-output-list");
        for (const input of instruction.inputs) {
          const inputEl = elementForOperand(input, searchInfo);
          inputs.appendChild(inputEl);
        }
        instEl.appendChild(inputs);
      }

      if (instruction.temps.length > 0) {
        const temps = createElement("div", "comma-sep-list input-output-list temps");
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

    const blockId = createElement("div", "block-id com clickable", block.id);
    blockId.onclick = mkBlockLinkHandler(block.id);
    sequenceBlock.appendChild(blockId);
    const blockPred = createElement("div", "predecessor-list block-list comma-sep-list");
    for (const pred of block.predecessors) {
      const predEl = createElement("div", "block-id com clickable", pred);
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
        const inputEl = elementForPhiOperand(input, this.searchInfo);
        phiEl.appendChild(inputEl);
      }
    }

    const instructions = createElement("div", "instructions");
    for (const instruction of block.instructions) {
      instructions.appendChild(elementForInstruction(instruction, this.searchInfo));
    }
    sequenceBlock.appendChild(instructions);
    const blockSucc = createElement("div", "successor-list block-list comma-sep-list");
    for (const succ of block.successors) {
      const succEl = createElement("div", "block-id com clickable", succ);
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

  addRangeView() {
    const preventRangeView = reason => {
      const toggleRangesInput = this.toggleRangeViewEl.firstChild as HTMLInputElement;
      if (this.rangeView) {
        toggleRangesInput.checked = false;
        this.toggleRangeView(toggleRangesInput);
      }
      toggleRangesInput.disabled = true;
      this.toggleRangeViewEl.style.textDecoration = "line-through";
      this.toggleRangeViewEl.setAttribute("title", reason);
    };

    if (this.sequence.register_allocation) {
      if (!this.rangeView) {
        this.rangeView = new RangeView(this);
      }
      const source = this.sequence.register_allocation;
      if (source.fixedLiveRanges.size == 0 && source.liveRanges.size == 0) {
        preventRangeView("No live ranges to show");
      } else if (this.numInstructions >= 249) {
        // This is due to the css grid-column being limited to 1000 columns.
        // Performance issues would otherwise impose some limit.
        // TODO(george.wort@arm.com): Allow the user to specify an instruction range
        //                            to display that spans less than 249 instructions.
        preventRangeView(
          "Live range display is only supported for sequences with less than 249 instructions");
      }
      if (this.showRangeView) {
        this.rangeView.initializeContent(this.sequence.blocks);
      }
    } else {
      preventRangeView("No live range data provided");
    }
  }

  elementForToggleRangeView() {
    const toggleRangeViewEl = createElement("label", "", "show live ranges");
    const toggleRangesInput = createElement("input", "range-toggle-show") as HTMLInputElement;
    toggleRangesInput.setAttribute("type", "checkbox");
    toggleRangesInput.oninput = () => this.toggleRangeView(toggleRangesInput);
    toggleRangeViewEl.insertBefore(toggleRangesInput, toggleRangeViewEl.firstChild);
    return toggleRangeViewEl;
  }

  toggleRangeView(toggleRangesInput: HTMLInputElement) {
    toggleRangesInput.disabled = true;
    this.showRangeView = toggleRangesInput.checked;
    if (this.showRangeView) {
      this.rangeView.initializeContent(this.sequence.blocks);
      this.rangeView.show();
    } else {
      this.rangeView.hide();
    }
    window.dispatchEvent(new Event('resize'));
    toggleRangesInput.disabled = false;
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
