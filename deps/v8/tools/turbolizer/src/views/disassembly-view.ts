// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../common/constants";
import { interpolate, storageGetItem, storageSetItem } from "../common/util";
import { ViewElements } from "../common/view-elements";
import { SelectionBroker } from "../selection/selection-broker";
import { TextView } from "./text-view";
import { SelectionMap } from "../selection/selection-map";
import { ClearableHandler, InstructionSelectionHandler } from "../selection/selection-handler";
import { TurbolizerInstructionStartInfo } from "../phases/instructions-phase";
import { SelectionStorage } from "../selection/selection-storage";
import { DisassemblyPhase } from "../phases/disassembly-phase";

const toolboxHTML =
  `<div id="disassembly-toolbox">
    <form>
      <label><input id="show-instruction-address" type="checkbox" name="instruction-address">Show addresses</label>
      <label><input id="show-instruction-binary" type="checkbox" name="instruction-binary">Show binary literal</label>
      <label><input id="highlight-gap-instructions" type="checkbox" name="instruction-binary">Highlight gap instructions</label>
    </form>
  </div>`;

export class DisassemblyView extends TextView {
  addrEventCounts: any;
  totalEventCounts: any;
  maxEventCounts: any;
  posLines: Array<number>;
  instructionSelectionHandler: InstructionSelectionHandler & ClearableHandler;
  offsetSelection: SelectionMap;
  showInstructionAddressHandler: () => void;
  showInstructionBinaryHandler: () => void;
  highlightGapInstructionsHandler: () => void;

  constructor(parent: HTMLDivElement, broker: SelectionBroker) {
    super(parent, broker);
    this.patterns = this.initializePatterns();

    this.divNode.addEventListener("click", e => this.linkClickHandler(e));
    this.divNode.addEventListener("click", e => this.linkBlockClickHandler(e));

    this.offsetSelection = new SelectionMap(offset => String(offset));
    this.instructionSelectionHandler = this.initializeInstructionSelectionHandler();
    this.broker.addInstructionHandler(this.instructionSelectionHandler);
    this.addDisassemblyToolbox();
  }

  public createViewElement(): HTMLDivElement {
    const pane = document.createElement("div");
    pane.setAttribute("id", C.DISASSEMBLY_PANE_ID);
    pane.innerHTML =
      `<pre id="disassembly-text-pre" class="prettyprint prettyprinted">
         <ul class="disassembly-list nolinenums noindent"></ul>
       </pre>`;
    return pane;
  }

  public updateSelection(scrollIntoView: boolean = false): void {
    if (this.divNode.parentNode == null) return;
    super.updateSelection(scrollIntoView, this.divNode.parentNode as HTMLElement);
    const selectedKeys = this.nodeSelections.current.selectedKeys();
    const keyPcOffsets: Array<TurbolizerInstructionStartInfo | string> = [
      ...this.sourceResolver.instructionsPhase.nodesToKeyPcOffsets(selectedKeys)
    ];
    if (this.offsetSelection) {
      for (const key of this.offsetSelection.selectedKeys()) {
        keyPcOffsets.push(key);
      }
    }
    const mkVisible = new ViewElements(this.divNode.parentElement);
    for (const keyPcOffset of keyPcOffsets) {
      const elementsToSelect = this.divNode.querySelectorAll(`[data-pc-offset='${keyPcOffset}']`);
      for (const el of elementsToSelect) {
        mkVisible.consider(el as HTMLElement, true);
        el.classList.toggle("selected", true);
      }
    }
    mkVisible.apply(scrollIntoView);
  }

  public processLine(line: string): Array<HTMLSpanElement> {
    let fragments = super.processLine(line);
    const cssCls = "prof";

    // Add profiling data per instruction if available.
    if (this.totalEventCounts) {
      const matches = /^(0x[0-9a-fA-F]+)\s+\d+\s+[0-9a-fA-F]+/.exec(line);
      if (matches) {
        const newFragments = new Array<HTMLSpanElement>();
        for (const event in this.addrEventCounts) {
          if (!this.addrEventCounts.hasOwnProperty(event)) continue;
          const count = this.addrEventCounts[event][matches[1]];
          if (count !== undefined) {
            const perc = count / this.totalEventCounts[event] * 100;
            let col = { r: 255, g: 255, b: 255 };

            for (let i = 0; i < C.PROF_COLS.length; i++) {
              if (perc === C.PROF_COLS[i].perc) {
                col = C.PROF_COLS[i].col;
                break;
              } else if (perc > C.PROF_COLS[i].perc && perc < C.PROF_COLS[i + 1].perc) {
                const col1 = C.PROF_COLS[i].col;
                const col2 = C.PROF_COLS[i + 1].col;
                const val = perc - C.PROF_COLS[i].perc;
                const max = C.PROF_COLS[i + 1].perc - C.PROF_COLS[i].perc;

                col.r = Math.round(interpolate(val, max, col1.r, col2.r));
                col.g = Math.round(interpolate(val, max, col1.g, col2.g));
                col.b = Math.round(interpolate(val, max, col1.b, col2.b));
                break;
              }
            }

            const fragment = this.createFragment(C.UNICODE_BLOCK, cssCls);
            fragment.title = `${event}: ${this.makeReadable(perc)} (${count})`;
            fragment.style.color = `rgb(${col.r}, ${col.g}, ${col.b})`;
            newFragments.push(fragment);
          } else {
            newFragments.push(this.createFragment(" ", cssCls));
          }
        }
        fragments = newFragments.concat(fragments);
      }
    }
    return fragments;
  }

  public detachSelection(): SelectionStorage {
    return null;
  }

  public adaptSelection(selection: SelectionStorage): SelectionStorage {
    return selection;
  }

  public searchInputAction(searchInput: HTMLInputElement, e: Event, onlyVisible: boolean): void {
    throw new Error("Method not implemented.");
  }

  public showContent(disassemblyPhase: DisassemblyPhase): void {
    console.time("disassembly-view");
    super.initializeContent(disassemblyPhase, null);
    this.showInstructionAddressHandler();
    this.showInstructionBinaryHandler();
    this.highlightGapInstructionsHandler();
    console.timeEnd("disassembly-view");
  }

  public initializeCode(sourceText: string, sourcePosition: number = 0): void {
    this.addrEventCounts = null;
    this.totalEventCounts = null;
    this.maxEventCounts = null;
    this.posLines = new Array<number>();
    // Comment lines for line 0 include sourcePosition already, only need to
    // add sourcePosition for lines > 0.
    this.posLines[0] = sourcePosition;
    if (sourceText && sourceText !== "") {
      const base = sourcePosition;
      let current = 0;
      const sourceLines = sourceText.split("\n");
      for (let i = 1; i < sourceLines.length; i++) {
        // Add 1 for newline character that is split off.
        current += sourceLines[i - 1].length + 1;
        this.posLines[i] = base + current;
      }
    }
  }

  public initializePerfProfile(eventCounts): void {
    if (eventCounts !== undefined) {
      this.addrEventCounts = eventCounts;
      this.totalEventCounts = {};
      this.maxEventCounts = {};
      for (const evName in this.addrEventCounts) {
        if (this.addrEventCounts.hasOwnProperty(evName)) {
          const keys = Object.keys(this.addrEventCounts[evName]);
          const values = keys.map(key => this.addrEventCounts[evName][key]);
          this.totalEventCounts[evName] = values.reduce((a, b) => a + b);
          this.maxEventCounts[evName] = values.reduce((a, b) => Math.max(a, b));
        }
      }
    } else {
      this.addrEventCounts = null;
      this.totalEventCounts = null;
      this.maxEventCounts = null;
    }
  }

  private initializeInstructionSelectionHandler(): InstructionSelectionHandler & ClearableHandler {
    const view = this;
    const broker = this.broker;
    return {
      select: function (instructionIds: Array<string>, selected: boolean) {
        view.offsetSelection.select(instructionIds, selected);
        view.updateSelection();
        broker.broadcastInstructionSelect(this, instructionIds.map(id => Number(id)), selected);
      },
      clear: function () {
        view.offsetSelection.clear();
        view.updateSelection();
        broker.broadcastClear(this);
      },
      brokeredInstructionSelect: function (instructionsOffsets: Array<[number, number]> |
        Array<Array<number>>, selected: boolean) {
        const firstSelect = view.offsetSelection.isEmpty();
        for (const instructionOffset of instructionsOffsets) {
          const keyPcOffsets = view.sourceResolver.instructionsPhase
            .instructionsToKeyPcOffsets(instructionOffset);
          view.offsetSelection.select(keyPcOffsets, selected);
        }
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.offsetSelection.clear();
        view.updateSelection();
      }
    };
  }

  private linkClickHandler(e: MouseEvent): void {
    if (!(e.target instanceof HTMLElement)) return;
    const offsetAsString = e.target.dataset.pcOffset !== undefined
      ? e.target.dataset.pcOffset
      : e.target.parentElement.dataset.pcOffset;
    const offset = Number.parseInt(offsetAsString, 10);
    if (offsetAsString !== undefined && !Number.isNaN(offset)) {
      this.offsetSelection.select([offset], true);
      const nodes = this.sourceResolver.instructionsPhase.nodesForPCOffset(offset);
      if (nodes.length > 0) {
        e.stopPropagation();
        if (!e.shiftKey) this.nodeSelectionHandler.clear();
        this.nodeSelectionHandler.select(nodes, true, false);
      } else {
        const instructions = this.sourceResolver.instructionsPhase.instructionsForPCOffset(offset);
        if (instructions.length > 0) {
          e.stopPropagation();
          if (!e.shiftKey) this.instructionSelectionHandler.clear();
          this.instructionSelectionHandler
              .brokeredInstructionSelect(instructions.map(instr => [instr, instr]), true);
          this.broker.broadcastInstructionSelect(this, instructions, true);
        } else {
          this.updateSelection();
        }
      }
    }
    return undefined;
  }

  private linkBlockClickHandler(e: MouseEvent): void {
    const spanBlockElement = e.target as HTMLSpanElement;
    const blockId = spanBlockElement.dataset.blockId;
    if (blockId !== undefined) {
      const blockIds = blockId.split(",");
      if (!e.shiftKey) this.blockSelectionHandler.clear();
      this.blockSelectionHandler.select(blockIds.map(id => Number(id)), true, false);
    }
  }

  private addDisassemblyToolbox(): void {
    const view = this;
    const toolbox = document.createElement("div");
    toolbox.id = "toolbox-anchor";
    toolbox.innerHTML = toolboxHTML;
    view.divNode.insertBefore(toolbox, view.divNode.firstChild);

    const instructionAddressInput: HTMLInputElement =
      view.divNode.querySelector("#show-instruction-address");

    instructionAddressInput.checked = storageGetItem("show-instruction-address");
    const showInstructionAddressHandler = () => {
      storageSetItem("show-instruction-address", instructionAddressInput.checked);
      for (const el of view.divNode.querySelectorAll(".instruction-address")) {
        el.classList.toggle("invisible", !instructionAddressInput.checked);
      }
    };
    instructionAddressInput.addEventListener("change", showInstructionAddressHandler);
    this.showInstructionAddressHandler = showInstructionAddressHandler;

    const instructionBinaryInput: HTMLInputElement =
      view.divNode.querySelector("#show-instruction-binary");

    instructionBinaryInput.checked = storageGetItem("show-instruction-binary");
    const showInstructionBinaryHandler = () => {
      storageSetItem("show-instruction-binary", instructionBinaryInput.checked);
      for (const el of view.divNode.querySelectorAll(".instruction-binary")) {
        el.classList.toggle("invisible", !instructionBinaryInput.checked);
      }
    };
    instructionBinaryInput.addEventListener("change", showInstructionBinaryHandler);
    this.showInstructionBinaryHandler = showInstructionBinaryHandler;

    const highlightGapInstructionsInput: HTMLInputElement =
      view.divNode.querySelector("#highlight-gap-instructions");

    highlightGapInstructionsInput.checked = storageGetItem("highlight-gap-instructions");
    const highlightGapInstructionsHandler = () => {
      storageSetItem("highlight-gap-instructions", highlightGapInstructionsInput.checked);
      view.divNode.classList.toggle("highlight-gap-instructions",
        highlightGapInstructionsInput.checked);
    };
    highlightGapInstructionsInput.addEventListener("change", highlightGapInstructionsHandler);
    this.highlightGapInstructionsHandler = highlightGapInstructionsHandler;
  }

  private makeReadable(num: number): string {
    // Shorten decimals and remove trailing zeroes for readability.
    return `${num.toFixed(3).replace(/\.?0+$/, "")}%`;
  }

  private initializePatterns(): Array<Array<any>> {
    return [
      [
        [/^0?x?[0-9a-fA-F]{8,16}\s+[0-9a-f]+\s+/, this.addressStyle, 1],
        [/^\s*--[^<]*<.*(not inlined|inlined\((\d+)\)):(\d+)>\s*--/, this.sourcePositionHeaderStyle, -1],
        [/^\s+-- B\d+ start.*/, this.blockHeaderStyle, -1],
        [/^.*/, this.unclassifiedStyle, -1]
      ],
      [
        [/^\s*[0-9a-f]+\s+/, this.numberStyle, 2],
        [/^\s*[0-9a-f]+\s+[0-9a-f]+\s+/, this.numberStyle, 2],
        [/^.*/, null, -1]
      ],
      [
        [/^REX.W \S+\s+/, this.opcodeStyle, 3],
        [/^\S+\s+/, this.opcodeStyle, 3],
        [/^\S+$/, this.opcodeStyle, -1],
        [/^.*/, null, -1]
      ],
      [
        [/^\s+/, null],
        [/^[^;]+$/, this.opcodeArgs, -1],
        [/^[^;]+/, this.opcodeArgs, 4],
        [/^;/, this.commentStyle, 5]
      ],
      [
        [/^.+$/, this.commentStyle, -1]
      ]
    ];
  }

  private get numberStyle(): { css: Array<string> } {
    return {
      css: ["instruction-binary", "lit"]
    };
  }

  private get opcodeStyle(): { css: string } {
    return {
      css: "kwd"
    };
  }

  private get unclassifiedStyle(): { css: string } {
    return {
      css: "com"
    };
  }

  private get commentStyle(): { css: string }  {
    return {
      css: "com"
    };
  }

  private get sourcePositionHeaderStyle(): { css: string } {
    return {
      css: "com"
    };
  }

  private get blockHeaderStyle(): { associateData(text: string, fragment: HTMLSpanElement) } {
    const view = this;
    return {
      associateData: (text: string, fragment: HTMLSpanElement) => {
        if (view.sourceResolver.disassemblyPhase.hasBlockStartInfo()) return false;
        const matches = /\d+/.exec(text);
        if (!matches) return true;
        const blockId = matches[0];
        fragment.dataset.blockId = blockId;
        fragment.innerHTML = text;
        fragment.className = "com block";
        return true;
      }
    };
  }

  private get opcodeArgs(): { associateData(text: string, fragment: HTMLSpanElement) } {
    const view = this;
    return {
      associateData: (text: string, fragment: HTMLSpanElement) => {
        fragment.innerHTML = text;
        const replacer = (match, hexOffset) => {
          const offset = Number.parseInt(hexOffset, 16);
          let keyOffset = view.sourceResolver.instructionsPhase.getKeyPcOffset(offset);
          if (keyOffset == -1) keyOffset = Number(offset);
          const blockIds = view.sourceResolver.disassemblyPhase.getBlockIdsForOffset(offset);
          let block = "";
          let blockIdData = "";
          if (blockIds && blockIds.length > 0) {
            block = `B${blockIds.join(",")} `;
            blockIdData = `data-block-id="${blockIds.join(",")}"`;
          }
          return `<span class="tag linkable-text" data-pc-offset="${keyOffset}" ${blockIdData}>${block}${match}</span>`;
        };
        fragment.innerHTML = text.replace(/<.0?x?([0-9a-fA-F]+)>/g, replacer);
        return true;
      }
    };
  }

  private get addressStyle(): { associateData(text: string, fragment: HTMLSpanElement) }  {
    const view = this;
    return {
      associateData: (text: string, fragment: HTMLSpanElement) => {
        const matches = text.match(/(?<address>0?x?[0-9a-fA-F]{8,16})(?<addressSpace>\s+)(?<offset>[0-9a-f]+)(?<offsetSpace>\s*)/);
        const offset = Number.parseInt(matches.groups["offset"], 16);
        const instructionKind = view.sourceResolver.instructionsPhase
          .getInstructionKindForPCOffset(offset);
        fragment.dataset.instructionKind = instructionKind;
        fragment.title = view.sourceResolver.instructionsPhase
          .instructionKindToReadableName(instructionKind);

        const blockIds = view.sourceResolver.disassemblyPhase.getBlockIdsForOffset(offset);
        const blockIdElement = document.createElement("span");
        blockIdElement.className = "block-id com linkable-text";
        blockIdElement.innerText = "";
        if (blockIds && blockIds.length > 0) {
          blockIds.forEach(blockId => view.addHtmlElementForBlockId(blockId, fragment));
          blockIdElement.innerText = `B${blockIds.join(",")}:`;
          blockIdElement.dataset.blockId = `${blockIds.join(",")}`;
        }
        fragment.appendChild(blockIdElement);

        const addressElement = document.createElement("span");
        addressElement.className = "instruction-address";
        addressElement.innerText = matches.groups["address"];
        const offsetElement = document.createElement("span");
        offsetElement.innerText = matches.groups["offset"];
        fragment.appendChild(addressElement);
        fragment.appendChild(document.createTextNode(matches.groups["addressSpace"]));
        fragment.appendChild(offsetElement);
        fragment.appendChild(document.createTextNode(matches.groups["offsetSpace"]));
        fragment.classList.add("tag");

        if (!Number.isNaN(offset)) {
          let pcOffset = view.sourceResolver.instructionsPhase.getKeyPcOffset(offset);
          if (pcOffset == -1) pcOffset = Number(offset);
          fragment.dataset.pcOffset = String(pcOffset);
          addressElement.classList.add("linkable-text");
          offsetElement.classList.add("linkable-text");
        }
        return true;
      }
    };
  }
}
