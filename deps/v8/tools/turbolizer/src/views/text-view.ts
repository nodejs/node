// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { isIterable } from "../common/util";
import { PhaseView } from "./view";
import { SelectionMap } from "../selection/selection-map";
import { SourceResolver } from "../source-resolver";
import { SelectionBroker } from "../selection/selection-broker";
import { ViewElements } from "../common/view-elements";
import { DisassemblyPhase } from "../phases/disassembly-phase";
import { SchedulePhase } from "../phases/schedule-phase";
import { SequencePhase } from "../phases/sequence-phase";
import {
  NodeSelectionHandler,
  BlockSelectionHandler,
  RegisterAllocationSelectionHandler,
  ClearableHandler
} from "../selection/selection-handler";

type GenericTextPhase = DisassemblyPhase | SchedulePhase | SequencePhase;
export abstract class TextView extends PhaseView {
  broker: SelectionBroker;
  sourceResolver: SourceResolver;
  nodeSelectionHandler: NodeSelectionHandler & ClearableHandler;
  blockSelectionHandler: BlockSelectionHandler & ClearableHandler;
  registerAllocationSelectionHandler: RegisterAllocationSelectionHandler & ClearableHandler;
  nodeSelection: SelectionMap;
  blockSelection: SelectionMap;
  registerAllocationSelection: SelectionMap;
  textListNode: HTMLUListElement;
  instructionIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  nodeIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  blockIdToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  blockIdToNodeIds: Map<string, Array<string>>;
  nodeIdToBlockId: Array<string>;
  patterns: Array<Array<any>>;

  constructor(parent: HTMLElement, broker: SelectionBroker) {
    super(parent);
    this.broker = broker;
    this.sourceResolver = broker.sourceResolver;
    this.textListNode = this.divNode.getElementsByTagName("ul")[0];
    this.instructionIdToHtmlElementsMap = new Map<string, Array<HTMLElement>>();
    this.nodeIdToHtmlElementsMap = new Map<string, Array<HTMLElement>>();
    this.blockIdToHtmlElementsMap = new Map<string, Array<HTMLElement>>();
    this.blockIdToNodeIds = new Map<string, Array<string>>();
    this.nodeIdToBlockId = new Array<string>();

    this.nodeSelection = new SelectionMap(node => String(node));
    this.blockSelection = new SelectionMap(block => String(block));
    this.registerAllocationSelection = new SelectionMap(register => String(register));

    this.nodeSelectionHandler = this.initializeNodeSelectionHandler();
    this.blockSelectionHandler = this.initializeBlockSelectionHandler();
    this.registerAllocationSelectionHandler = this.initializeRegisterAllocationSelectionHandler();

    broker.addNodeHandler(this.nodeSelectionHandler);
    broker.addBlockHandler(this.blockSelectionHandler);
    broker.addRegisterAllocatorHandler(this.registerAllocationSelectionHandler);

    this.divNode.addEventListener("click", e => {
      if (!e.shiftKey) {
        this.nodeSelectionHandler.clear();
      }
      e.stopPropagation();
    });
  }

  public initializeContent(genericPhase: GenericTextPhase, _): void {
    this.clearText();
    if (genericPhase instanceof DisassemblyPhase) {
      this.processText(genericPhase.data);
    }
    this.show();
  }

  public updateSelection(scrollIntoView: boolean = false): void {
    if (this.divNode.parentNode == null) return;
    const mkVisible = new ViewElements(this.divNode.parentNode as HTMLElement);
    const elementsToSelect = this.divNode.querySelectorAll(`[data-pc-offset]`);

    for (const el of elementsToSelect) {
      el.classList.toggle("selected", false);
    }
    for (const [blockId, elements] of this.blockIdToHtmlElementsMap.entries()) {
      const isSelected = this.blockSelection.isSelected(blockId);
      for (const element of elements) {
        mkVisible.consider(element, isSelected);
        element.classList.toggle("selected", isSelected);
      }
    }

    for (const key of this.instructionIdToHtmlElementsMap.keys()) {
      for (const element of this.instructionIdToHtmlElementsMap.get(key)) {
        element.classList.toggle("selected", false);
      }
    }
    for (const instrId of this.registerAllocationSelection.selectedKeys()) {
      const elements = this.instructionIdToHtmlElementsMap.get(instrId);
      if (!elements) continue;
      for (const element of elements) {
        mkVisible.consider(element, true);
        element.classList.toggle("selected", true);
      }
    }

    for (const key of this.nodeIdToHtmlElementsMap.keys()) {
      for (const element of this.nodeIdToHtmlElementsMap.get(key)) {
        element.classList.toggle("selected", false);
      }
    }
    for (const nodeId of this.nodeSelection.selectedKeys()) {
      const elements = this.nodeIdToHtmlElementsMap.get(nodeId);
      if (!elements) continue;
      for (const element of elements) {
        mkVisible.consider(element, true);
        element.classList.toggle("selected", true);
      }
    }

    mkVisible.apply(scrollIntoView);
  }

  public processLine(line: string): Array<HTMLSpanElement> {
    const fragments = new Array<HTMLSpanElement>();
    let patternSet = 0;
    while (true) {
      const beforeLine = line;
      for (const pattern of this.patterns[patternSet]) {
        const matches = line.match(pattern[0]);
        if (matches) {
          if (matches[0].length > 0) {
            const style = pattern[1] != null ? pattern[1] : {};
            const text = matches[0];
            if (text.length > 0) {
              const fragment = this.createFragment(matches[0], style);
              if (fragment !== null) fragments.push(fragment);
            }
            line = line.substr(matches[0].length);
          }
          let nextPatternSet = patternSet;
          if (pattern.length > 2) {
            nextPatternSet = pattern[2];
          }
          if (line.length == 0) {
            if (nextPatternSet != -1) {
              throw (`illegal parsing state in text-view in patternSet: ${patternSet}`);
            }
            return fragments;
          }
          patternSet = nextPatternSet;
          break;
        }
      }
      if (beforeLine == line) {
        throw (`input not consumed in text-view in patternSet: ${patternSet}`);
      }
    }
  }

  public onresize(): void {}

  // instruction-id are the divs for the register allocator phase
  protected addHtmlElementForInstructionId(anyInstructionId: any, htmlElement: HTMLElement): void {
    const instructionId = String(anyInstructionId);
    if (!this.instructionIdToHtmlElementsMap.has(instructionId)) {
      this.instructionIdToHtmlElementsMap.set(instructionId, new Array<HTMLElement>());
    }
    this.instructionIdToHtmlElementsMap.get(instructionId).push(htmlElement);
  }

  protected addHtmlElementForNodeId(anyNodeId: any, htmlElement: HTMLElement): void {
    const nodeId = String(anyNodeId);
    if (!this.nodeIdToHtmlElementsMap.has(nodeId)) {
      this.nodeIdToHtmlElementsMap.set(nodeId, new Array<HTMLElement>());
    }
    this.nodeIdToHtmlElementsMap.get(nodeId).push(htmlElement);
  }

  protected addHtmlElementForBlockId(anyBlockId: any, htmlElement: HTMLElement): void {
    const blockId = String(anyBlockId);
    if (!this.blockIdToHtmlElementsMap.has(blockId)) {
      this.blockIdToHtmlElementsMap.set(blockId, new Array<HTMLElement>());
    }
    this.blockIdToHtmlElementsMap.get(blockId).push(htmlElement);
  }

  protected createFragment(text: string, style): HTMLSpanElement {
    const fragment = document.createElement("span");

    if (typeof style.associateData === "function") {
      if (!style.associateData(text, fragment)) return null;
    } else {
      if (style.css !== undefined) {
        const css = isIterable(style.css) ? style.css : [style.css];
        for (const cls of css) {
          fragment.classList.add(cls);
        }
      }
      fragment.innerText = text;
    }

    return fragment;
  }

  private initializeNodeSelectionHandler(): NodeSelectionHandler & ClearableHandler {
    const view = this;
    return {
      select: function (nodeIds: Array<string | number>, selected: boolean) {
        view.nodeSelection.select(nodeIds, selected);
        view.updateSelection();
        view.broker.broadcastNodeSelect(this, view.nodeSelection.selectedKeys(), selected);
      },
      clear: function () {
        view.nodeSelection.clear();
        view.updateSelection();
        view.broker.broadcastClear(this);
      },
      brokeredNodeSelect: function (nodeIds: Set<string>, selected: boolean) {
        const firstSelect = view.blockSelection.isEmpty();
        view.nodeSelection.select(nodeIds, selected);
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.nodeSelection.clear();
        view.updateSelection();
      }
    };
  }

  private initializeBlockSelectionHandler(): BlockSelectionHandler & ClearableHandler {
    const view = this;
    return {
      select: function (blockIds: Array<string>, selected: boolean) {
        view.blockSelection.select(blockIds, selected);
        view.updateSelection();
        view.broker.broadcastBlockSelect(this, blockIds, selected);
      },
      clear: function () {
        view.blockSelection.clear();
        view.updateSelection();
        view.broker.broadcastClear(this);
      },
      brokeredBlockSelect: function (blockIds: Array<string>, selected: boolean) {
        const firstSelect = view.blockSelection.isEmpty();
        view.blockSelection.select(blockIds, selected);
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.blockSelection.clear();
        view.updateSelection();
      }
    };
  }

  private initializeRegisterAllocationSelectionHandler(): RegisterAllocationSelectionHandler
    & ClearableHandler {
    const view = this;
    return {
      select: function (instructionIds: Array<number>, selected: boolean) {
        view.registerAllocationSelection.select(instructionIds, selected);
        view.updateSelection();
        view.broker.broadcastInstructionSelect(null, instructionIds, selected);
      },
      clear: function () {
        view.registerAllocationSelection.clear();
        view.updateSelection();
        view.broker.broadcastClear(this);
      },
      brokeredRegisterAllocationSelect: function (instructionsOffsets: Array<[number, number]>,
                                                  selected: boolean) {
        const firstSelect = view.blockSelection.isEmpty();
        for (const instructionOffset of instructionsOffsets) {
          view.registerAllocationSelection.select(instructionOffset, selected);
        }
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.registerAllocationSelection.clear();
        view.updateSelection();
      }
    };
  }

  private clearText(): void {
    while (this.textListNode.firstChild) {
      this.textListNode.removeChild(this.textListNode.firstChild);
    }
  }

  private processText(text: string): void {
    const textLines = text.split(/[\n]/);
    let lineNo = 0;
    for (const line of textLines) {
      const li = document.createElement("li");
      li.className = "nolinenums";
      li.dataset.lineNo = String(lineNo++);
      const fragments = this.processLine(line);
      for (const fragment of fragments) {
        li.appendChild(fragment);
      }
      this.textListNode.appendChild(li);
    }
  }
}
