// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./../common/constants";
import { createElement, storageSetItem } from "../common/util";
import { TextView } from "./text-view";
import { RangeView } from "./range-view";
import { SelectionStorage } from "../selection/selection-storage";
import { SelectionBroker } from "../selection/selection-broker";
import {
  SequenceBlock,
  SequenceBlockInstruction,
  SequenceBlockOperand,
  SequencePhase
} from "../phases/sequence-phase";

export class SequenceView extends TextView {
  sequence: SequencePhase;
  rangeView: RangeView;
  numInstructions: number;
  currentPhaseIndex: number;
  searchInfo: Array<string>;
  phaseIndexes: Set<number>;
  isShown: boolean;
  showRangeView: boolean;
  phaseSelectEl: HTMLSelectElement;
  toggleRangeViewEl: HTMLElement;
  firstInstrInput: HTMLInputElement;
  lastInstrInput: HTMLInputElement;

  constructor(parent: HTMLElement, broker: SelectionBroker) {
    super(parent, broker);
    this.numInstructions = 0;
    this.phaseIndexes = new Set<number>();
    this.isShown = false;
    this.showRangeView = false;
    this.toggleRangeViewEl = this.elementForToggleRangeView();
  }

  public createViewElement(): HTMLElement {
    const pane = document.createElement("div");
    pane.setAttribute("id", C.SEQUENCE_PANE_ID);
    pane.classList.add("scrollable");
    pane.setAttribute("tabindex", "0");
    return pane;
  }

  public detachSelection(): SelectionStorage {
    return new SelectionStorage(this.nodeSelections.current.detachSelection(),
                                this.blockSelections.current.detachSelection(),
                                this.instructionSelections.current.detachSelection());
  }

  public adaptSelection(selection: SelectionStorage): SelectionStorage {
    for (const key of selection.nodes.keys()) selection.adaptedNodes.add(key);
    for (const key of selection.blocks.keys()) selection.adaptedBocks.add(key);
    for (const key of selection.instructions.keys()) selection.adaptedInstructions.add(Number(key));
    return selection;
  }

  public show(): void {
    this.currentPhaseIndex = this.phaseSelectEl.selectedIndex;
    if (!this.isShown) {
      this.isShown = true;
      this.phaseIndexes.add(this.currentPhaseIndex);
      this.container.appendChild(this.divNode);
      this.container.getElementsByClassName("graph-toolbox")[0].appendChild(this.toggleRangeViewEl);
    }
    if (this.showRangeView) this.rangeView.show();
  }

  public hide(): void {
    // A single SequenceView object is used for two phases (i.e before and after
    // register allocation), tracking the indexes lets the redundant hides and
    // shows be avoided when switching between the two.
    this.currentPhaseIndex = this.phaseSelectEl.selectedIndex;
    if (!this.phaseIndexes.has(this.currentPhaseIndex)) {
      this.isShown = false;
      this.container.removeChild(this.divNode);
      this.container.getElementsByClassName("graph-toolbox")[0].removeChild(this.toggleRangeViewEl);
      if (this.showRangeView) this.rangeView.hide();
    }
  }

  public onresize(): void {
    if (this.showRangeView) this.rangeView.onresize();
  }

  public initializeContent(sequence: SequencePhase, rememberedSelection: SelectionStorage): void {
    this.divNode.innerHTML = "";
    const view = this;
    this.divNode.onclick = (e: MouseEvent) => {
      if (view.showRangeView) view.rangeView.focusHandler.clearCoordsInFocus();
    };
    this.sequence = sequence;
    this.searchInfo = new Array<string>();
    this.phaseSelectEl = document.getElementById("phase-select") as HTMLSelectElement;
    this.currentPhaseIndex = this.phaseSelectEl.selectedIndex;
    this.addBlocks(this.sequence.blocks);
    this.numInstructions = this.sequence.getNumInstructions();
    this.addRangeView();
    this.show();
    if (rememberedSelection) {
      const adaptedSelection = this.adaptSelection(rememberedSelection);
      if (adaptedSelection.isAdapted()) this.attachSelection(adaptedSelection);
    }
  }

  public searchInputAction(searchBar: HTMLInputElement, e: KeyboardEvent): void {
    e.stopPropagation();
    this.nodeSelectionHandler.clear();
    const query = searchBar.value;
    if (query.length == 0) return;
    const select = new Array<string>();
    storageSetItem("lastSearch", query);
    const reg = new RegExp(query);
    for (const item of this.searchInfo) {
      if (reg.exec(item) != null) {
        select.push(item);
      }
    }
    this.nodeSelectionHandler.select(select, true, false);
  }

  private attachSelection(adaptedSelection: SelectionStorage): void {
    if (!(adaptedSelection instanceof SelectionStorage)) return;
    this.blockSelectionHandler.clear();
    this.nodeSelectionHandler.clear();
    this.registerAllocationSelectionHandler.clear();
    this.blockSelectionHandler.select(
                Array.from(adaptedSelection.adaptedBocks).map(block => Number(block)), true, true);
    this.nodeSelectionHandler.select(adaptedSelection.adaptedNodes, true, true);
    this.registerAllocationSelectionHandler.select(Array.from(adaptedSelection.adaptedInstructions),
                                                   true, true);
  }

  private addBlocks(blocks: Array<SequenceBlock>): void {
    for (const block of blocks) {
      const blockEl = this.elementForBlock(block);
      this.divNode.appendChild(blockEl);
    }
  }

  private elementForBlock(block: SequenceBlock): HTMLElement {
    const sequenceBlock = createElement("div", "schedule-block");
    sequenceBlock.classList.toggle("deferred", block.deferred);

    const blockIdEl = createElement("div", "block-id com clickable", String(block.id));
    // Select just the block id when any of the block's instructions or positions
    // are selected.
    this.addHtmlElementForBlockId(this.getSubId(block.id), blockIdEl);
    blockIdEl.onclick = this.mkBlockLinkHandler(block.id);
    sequenceBlock.appendChild(blockIdEl);

    const blockPred = createElement("div", "predecessor-list block-list comma-sep-list");
    for (const pred of block.predecessors) {
      const predEl = createElement("div", "block-id com clickable", String(pred));
      predEl.onclick = this.mkBlockLinkHandler(pred);
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

      const outputEl = this.elementForOperand(phi.output);
      phiEl.appendChild(outputEl);

      const assignEl = createElement("div", "assign", "=");
      phiEl.appendChild(assignEl);

      for (const input of phi.operands) {
        const inputEl = this.elementForPhiOperand(input);
        phiEl.appendChild(inputEl);
      }
    }

    const instructions = createElement("div", "instructions");
    for (const instruction of block.instructions) {
      instructions.appendChild(this.elementForInstruction(instruction));
    }
    sequenceBlock.appendChild(instructions);

    const blockSucc = createElement("div", "successor-list block-list comma-sep-list");
    for (const succ of block.successors) {
      const succEl = createElement("div", "block-id com clickable", String(succ));
      succEl.onclick = this.mkBlockLinkHandler(succ);
      blockSucc.appendChild(succEl);
    }
    if (block.successors.length > 0) sequenceBlock.appendChild(blockSucc);

    this.addHtmlElementForBlockId(block.id, sequenceBlock);
    return sequenceBlock;
  }

  private elementForOperand(operand: SequenceBlockOperand): HTMLElement {
    let isVirtual = false;
    let className = `parameter tag clickable ${operand.type}`;
    if (operand.text[0] === "v" && !(operand.tooltip && operand.tooltip.includes("Float"))) {
      isVirtual = true;
      className += " virtual-reg";
    }
    const span = createElement("span", className, operand.text);
    if (operand.tooltip) {
      span.setAttribute("title", operand.tooltip);
    }
    return this.elementForOperandWithSpan(span, operand.text, isVirtual);
  }

  private elementForPhiOperand(text: string): HTMLElement {
    const span = createElement("span", "parameter tag clickable virtual-reg", text);
    return this.elementForOperandWithSpan(span, text, true);
  }

  private elementForOperandWithSpan(span: HTMLSpanElement, text: string, isVirtual: boolean):
    HTMLElement {
    const selectionText = isVirtual ? `virt_${text}` : text;
    const onclick = this.mkOperandLinkHandler(selectionText);
    const view = this;
    if (isVirtual) {
      const row = parseInt(text.substring(1), 10);
      span.onclick = function (e) {
        onclick(e);
        if (view.showRangeView) view.rangeView.focusHandler.setFocusVirtualRegister(row);
      };
    } else {
      span.onclick = onclick;
    }
    this.searchInfo.push(text);
    this.addHtmlElementForNodeId(selectionText, span);
    const container = createElement("div", "");
    container.appendChild(span);
    return container;
  }

  private elementForInstruction(instruction: SequenceBlockInstruction): HTMLElement {
    const instNodeEl = createElement("div", "instruction-node");

    const instId = createElement("div", "instruction-id", String(instruction.id));
    const offsets = this.sourceResolver.instructionsPhase.instructionToPcOffsets(instruction.id);
    instId.classList.add("clickable");
    // Select instruction id for both when the instruction is selected and when any of its
    // positions are selected.
    this.addHtmlElementForInstructionId(instruction.id, instId);
    this.addHtmlElementForInstructionId(this.getSubId(instruction.id), instId);
    instId.onclick = this.mkInstructionLinkHandler(instruction.id);
    instId.dataset.instructionId = String(instruction.id);
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
      for (const [destination, source] of gap) {
        hasGaps = true;
        const moveEl = createElement("div", "move");
        const destinationEl = this.elementForOperand(destination);
        moveEl.appendChild(destinationEl);
        const assignEl = createElement("div", "assign", "=");
        moveEl.appendChild(assignEl);
        const sourceEl = this.elementForOperand(source);
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
        const outputEl = this.elementForOperand(output);
        outputs.appendChild(outputEl);
      }
      instEl.appendChild(outputs);
      const assignEl = createElement("div", "assign", "=");
      instEl.appendChild(assignEl);
    }

    const text = instruction.opcode + instruction.flags;
    const instLabel = createElement("div", "node-label", text);
    if (instruction.opcode == "ArchNop" && instruction.outputs.length == 1
      && instruction.outputs[0].tooltip) {
      instLabel.innerText = instruction.outputs[0].tooltip;
    }

    this.searchInfo.push(text);
    this.addHtmlElementForNodeId(text, instLabel);
    instEl.appendChild(instLabel);

    if (instruction.inputs.length > 0) {
      const inputs = createElement("div", "comma-sep-list input-output-list");
      for (const input of instruction.inputs) {
        const inputEl = this.elementForOperand(input);
        inputs.appendChild(inputEl);
      }
      instEl.appendChild(inputs);
    }

    if (instruction.temps.length > 0) {
      const temps = createElement("div", "comma-sep-list input-output-list temps");
      for (const temp of instruction.temps) {
        const tempEl = this.elementForOperand(temp);
        temps.appendChild(tempEl);
      }
      instEl.appendChild(temps);
    }

    return instNodeEl;
  }

  private addRangeView(): void {
    if (this.sequence.registerAllocation) {
      if (!this.rangeView) {
        this.rangeView = new RangeView(this, parseInt(this.firstInstrInput.value, 10),
                                       parseInt(this.lastInstrInput.value, 10));
      }
      const source = this.sequence.registerAllocation;
      if (source.fixedLiveRanges.length == 0 && source.liveRanges.length == 0) {
        this.preventRangeView("No live ranges to show");
      }
      if (this.showRangeView) {
        this.rangeView.initializeContent(this.sequence.blocks, this.broker);
      }
    } else {
      this.preventRangeView("No live range data provided");
    }
  }

  private preventRangeView(reason: string): void {
    const toggleRangesInput = this.toggleRangeViewEl.firstChild as HTMLInputElement;
    if (this.rangeView) {
      toggleRangesInput.checked = false;
      this.toggleRangeView(toggleRangesInput);
    }
    toggleRangesInput.disabled = true;
    this.toggleRangeViewEl.style.textDecoration = "line-through";
    this.toggleRangeViewEl.setAttribute("title", reason);
  }

  private mkBlockLinkHandler(blockId: number): (e: MouseEvent) => void {
    return this.mkLinkHandler(blockId, this.blockSelectionHandler);
  }

  private mkInstructionLinkHandler(instrId: number): (e: MouseEvent) => void {
    return this.mkLinkHandler(instrId, this.registerAllocationSelectionHandler);
  }

  private mkOperandLinkHandler(text: string): (e: MouseEvent) => void {
    return this.mkLinkHandler(text, this.nodeSelectionHandler);
  }

  private mkLinkHandler(id: string | number, handler): (e: MouseEvent) => void {
    return function (e: MouseEvent) {
      e.stopPropagation();
      if (!e.shiftKey) {
        handler.clear();
      }
      handler.select([id], true);
    };
  }

  private elementForRangeViewInputElement(form: HTMLElement, text: string): HTMLInputElement {
    const instrInputEl = createElement("input", "instruction-range-input") as HTMLInputElement;
    instrInputEl.type = "text";
    instrInputEl.title = text;
    instrInputEl.placeholder = text;
    instrInputEl.alt = text;
    form.appendChild(instrInputEl);
    return instrInputEl;
  }

  private elementForToggleRangeView(): HTMLElement {
    const toggleRangeViewEl = createElement("label", "", "show live ranges from ");

    const form = createElement("form", "range-toggle-form");
    toggleRangeViewEl.appendChild(form);

    this.firstInstrInput = this.elementForRangeViewInputElement(form, "first instruction");
    form.appendChild(createElement("span", "", " to "));
    this.lastInstrInput = this.elementForRangeViewInputElement(form, "last instruction");

    const submit = createElement("input", "instruction-range-submit") as HTMLInputElement;
    submit.type = "submit";
    submit.value = "Refresh Ranges";
    submit.onclick = (e: MouseEvent) => {
      e.preventDefault();
      // Single click if not shown, double click to refresh if shown.
      this.toggleRangeViewEl.click();
      if (!this.showRangeView) this.toggleRangeViewEl.click();
    };
    form.appendChild(submit);

    const toggleRangesInput = createElement("input", "range-toggle-show") as HTMLInputElement;
    toggleRangesInput.setAttribute("type", "checkbox");
    toggleRangesInput.oninput = () => this.toggleRangeView(toggleRangesInput);
    toggleRangeViewEl.insertBefore(toggleRangesInput, toggleRangeViewEl.firstChild);
    return toggleRangeViewEl;
  }

  private toggleRangeView(toggleRangesInput: HTMLInputElement): void {
    toggleRangesInput.disabled = true;
    this.showRangeView = toggleRangesInput.checked;
    if (this.showRangeView) {
      const firstInstr = parseInt(this.firstInstrInput.value, 10);
      const lastInstr = parseInt(this.lastInstrInput.value, 10);
      if (this.rangeView.instructionRangeHandler.isNewRangeViewRequired(firstInstr, lastInstr)) {
        // Remove current RangeView's selection nodes and blocks from SelectionHandlers.
        this.removeHtmlElementFromAllMapsIf((e: HTMLElement) =>
                                             e.closest("#ranges-content") != null);
        this.rangeView = new RangeView(this, firstInstr, lastInstr);
        this.addRangeView();
      }
      this.rangeView.initializeContent(this.sequence.blocks, this.broker);
      this.rangeView.show();
    } else {
      this.rangeView.hide();
    }
    window.dispatchEvent(new Event("resize"));
    toggleRangesInput.disabled = false;
  }
}
