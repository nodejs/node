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

class SelectionMapsHandler {
  view: TextView;
  idToHtmlElementsMap: Map<string, Array<HTMLElement>>;
  current: SelectionMap;
  previous: SelectionMap;

  constructor(view: TextView, idToHtmlElementsMap: Map<string, Array<HTMLElement>>) {
    this.view = view;
    this.idToHtmlElementsMap = idToHtmlElementsMap;
    this.current = new SelectionMap(id => String(id));
    this.previous = new SelectionMap(id => String(id));
  }

  public clearCurrent(): void {
    this.previous.selection = this.current.selection;
    this.current.clear();
  }

  public clearPrevious(): void {
    for (const blockId of this.previous.selectedKeys()) {
      const elements = this.idToHtmlElementsMap.get(blockId);
      if (!elements) continue;
      for (const element of elements) {
        element.classList.toggle("selected", false);
      }
    }
    this.previous.clear();
  }

  public selectElements(scrollIntoView: boolean, scrollDiv: HTMLElement): void {
    const mkVisible = new ViewElements(scrollDiv);
    for (const id of this.current.selectedKeys()) {
      const elements = this.idToHtmlElementsMap.get(id);
      if (!elements) continue;
      for (const element of elements) {
        if (element.className.substring(0, 5) != "range" && element.getRootNode() == document) {
          mkVisible.consider(element, true);
        }
        element.classList.toggle("selected", true);
      }
    }
    mkVisible.apply(scrollIntoView);
  }
}
export abstract class TextView extends PhaseView {
  broker: SelectionBroker;
  sourceResolver: SourceResolver;
  nodeSelectionHandler: NodeSelectionHandler & ClearableHandler;
  blockSelectionHandler: BlockSelectionHandler & ClearableHandler;
  registerAllocationSelectionHandler: RegisterAllocationSelectionHandler & ClearableHandler;
  selectionCleared: boolean;
  nodeSelections: SelectionMapsHandler;
  instructionSelections: SelectionMapsHandler;
  blockSelections: SelectionMapsHandler;
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

    this.nodeSelections = new SelectionMapsHandler(this, this.nodeIdToHtmlElementsMap);
    this.instructionSelections = new SelectionMapsHandler(this, this.instructionIdToHtmlElementsMap);
    this.blockSelections = new SelectionMapsHandler(this, this.blockIdToHtmlElementsMap);

    this.nodeSelectionHandler = this.initializeNodeSelectionHandler();
    this.blockSelectionHandler = this.initializeBlockSelectionHandler();
    this.registerAllocationSelectionHandler = this.initializeRegisterAllocationSelectionHandler();

    broker.addNodeHandler(this.nodeSelectionHandler);
    broker.addBlockHandler(this.blockSelectionHandler);
    broker.addRegisterAllocatorHandler(this.registerAllocationSelectionHandler);

    this.selectionCleared = false;

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

  public clearSelectionMaps() {
    this.instructionIdToHtmlElementsMap.clear();
    this.nodeIdToHtmlElementsMap.clear();
    this.blockIdToHtmlElementsMap.clear();
  }

  public updateSelection(scrollIntoView: boolean = false,
                         scrollDiv: HTMLElement = this.divNode as HTMLElement): void {
    if (this.divNode.parentNode == null) return;

    const clearDisassembly = () => {
      const elementsToSelect = this.divNode.querySelectorAll(`[data-pc-offset]`);
      for (const el of elementsToSelect) {
        el.classList.toggle("selected", false);
      }
    };

    clearDisassembly();
    this.blockSelections.clearPrevious();
    this.instructionSelections.clearPrevious();
    this.nodeSelections.clearPrevious();
    this.blockSelections.selectElements(scrollIntoView, scrollDiv);
    this.instructionSelections.selectElements(scrollIntoView, scrollDiv);
    this.nodeSelections.selectElements(scrollIntoView, scrollDiv);
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

  private removeHtmlElementFromMapIf(condition: (e: HTMLElement) => boolean,
                                     map: Map<string, Array<HTMLElement>>): void {
    for (const [nodeId, elements] of map) {
      let i = elements.length;
      while (i--) {
        if (condition(elements[i])) {
          elements.splice(i, 1);
        }
      }
      if (elements.length == 0) {
        map.delete(nodeId);
      }
    }
  }

  public removeHtmlElementFromAllMapsIf(condition: (e: HTMLElement) => boolean): void {
    this.clearSelection();
    this.removeHtmlElementFromMapIf(condition, this.nodeIdToHtmlElementsMap);
    this.removeHtmlElementFromMapIf(condition, this.blockIdToHtmlElementsMap);
    this.removeHtmlElementFromMapIf(condition, this.instructionIdToHtmlElementsMap);
  }

  public clearSelection(): void {
    if (this.selectionCleared) return;
    this.blockSelections.clearCurrent();
    this.instructionSelections.clearCurrent();
    this.nodeSelections.clearCurrent();
    this.updateSelection();
    this.selectionCleared = true;
  }

  // instruction-id are the divs for the register allocator phase
  public addHtmlElementForInstructionId(anyInstructionId: any, htmlElement: HTMLElement): void {
    const instructionId = String(anyInstructionId);
    if (!this.instructionIdToHtmlElementsMap.has(instructionId)) {
      this.instructionIdToHtmlElementsMap.set(instructionId, new Array<HTMLElement>());
    }
    this.instructionIdToHtmlElementsMap.get(instructionId).push(htmlElement);
  }

  public addHtmlElementForNodeId(anyNodeId: any, htmlElement: HTMLElement): void {
    const nodeId = String(anyNodeId);
    if (!this.nodeIdToHtmlElementsMap.has(nodeId)) {
      this.nodeIdToHtmlElementsMap.set(nodeId, new Array<HTMLElement>());
    }
    this.nodeIdToHtmlElementsMap.get(nodeId).push(htmlElement);
  }

  public addHtmlElementForBlockId(anyBlockId: any, htmlElement: HTMLElement): void {
    const blockId = String(anyBlockId);
    if (!this.blockIdToHtmlElementsMap.has(blockId)) {
      this.blockIdToHtmlElementsMap.set(blockId, new Array<HTMLElement>());
    }
    this.blockIdToHtmlElementsMap.get(blockId).push(htmlElement);
  }

  public getSubId(id: number): number {
    return -id - 1;
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

  private selectionHandlerSelect(selection: SelectionMap, ids: Array<any>,
    selected: boolean, scrollIntoView: boolean = false): void {
    this.selectionCleared = false;
    const firstSelect = scrollIntoView ? this.blockSelections.current.isEmpty() : false;
    selection.select(ids, selected);
    this.updateSelection(firstSelect);
  }

  private selectionHandlerClear(): void {
    this.clearSelection();
    this.broker.broadcastClear(this);
  }

  private initializeNodeSelectionHandler(): NodeSelectionHandler & ClearableHandler {
    const view = this;
    return {
      select: function (nodeIds: Array<string | number>, selected: boolean,
                        scrollIntoView: boolean) {
        view.selectionHandlerSelect(view.nodeSelections.current, nodeIds, selected, scrollIntoView);
        view.broker.broadcastNodeSelect(this, view.nodeSelections.current.selectedKeys(), selected);
      },
      brokeredNodeSelect: function (nodeIds: Set<string>, selected: boolean) {
        view.selectionHandlerSelect(view.nodeSelections.current,
                                    Array.from(nodeIds), selected, false);
      },
      clear: view.selectionHandlerClear.bind(this),
      brokeredClear: view.clearSelection.bind(this),
    };
  }

  private initializeBlockSelectionHandler(): BlockSelectionHandler & ClearableHandler {
    const view = this;
    return {
      select: function (blockIds: Array<number>, selected: boolean) {
        view.selectionHandlerSelect(view.blockSelections.current, blockIds, selected);
        view.broker.broadcastBlockSelect(this,
                    Array.from(view.blockSelections.current.selectedKeysAsAbsNumbers()), selected);
      },
      brokeredBlockSelect: function (blockIds: Array<number>, selected: boolean) {
        view.selectionHandlerSelect(view.blockSelections.current, blockIds, selected, true);
      },
      clear: view.selectionHandlerClear.bind(this),
      brokeredClear: view.clearSelection.bind(this),
    };
  }

  private initializeRegisterAllocationSelectionHandler(): RegisterAllocationSelectionHandler
    & ClearableHandler {
    const view = this;
    return {
      select: function (instructionIds: Array<number>, selected: boolean, scrollIntoView: boolean) {
        view.selectionHandlerSelect(view.instructionSelections.current, instructionIds, selected,
                                    scrollIntoView);
        view.broker.broadcastInstructionSelect(this,
              Array.from(view.instructionSelections.current.selectedKeysAsAbsNumbers()), selected);
      },
      brokeredRegisterAllocationSelect: function (instructionsOffsets: Array<[number, number]>,
                                                  selected: boolean) {
        view.selectionCleared = false;
        const firstSelect = view.blockSelections.current.isEmpty();
        for (const instructionOffset of instructionsOffsets) {
          view.instructionSelections.current.select(instructionOffset, selected);
        }
        view.updateSelection(firstSelect);
      },
      clear: view.selectionHandlerClear.bind(this),
      brokeredClear: view.clearSelection.bind(this),
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
