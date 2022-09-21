// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Source } from "../source";
import { GenericPosition, SourceResolver } from "../source-resolver";
import { SelectionBroker } from "../selection/selection-broker";
import { CodeMode, View } from "./view";
import { SelectionMap } from "../selection/selection-map";
import { ViewElements } from "../common/view-elements";
import { ClearableHandler, SourcePositionSelectionHandler } from "../selection/selection-handler";
import { SourcePosition } from "../position";

interface PR {
  prettyPrint(_: unknown, el: HTMLElement): void;
}

declare global {
  const PR: PR;
}

export class CodeView extends View {
  broker: SelectionBroker;
  source: Source;
  sourceResolver: SourceResolver;
  codeMode: CodeMode;
  sourcePositionToHtmlElements: Map<string, Array<HTMLElement>>;
  showAdditionalInliningPosition: boolean;
  sourcePositionSelection: SelectionMap;
  sourcePositionSelectionHandler: SourcePositionSelectionHandler & ClearableHandler;

  constructor(parent: HTMLElement, broker: SelectionBroker,  sourceFunction: Source,
              sourceResolver: SourceResolver, codeMode: CodeMode) {
    super(parent);
    this.broker = broker;
    this.source = sourceFunction;
    this.sourceResolver = sourceResolver;
    this.codeMode = codeMode;
    this.sourcePositionToHtmlElements = new Map<string, Array<HTMLElement>>();
    this.showAdditionalInliningPosition = false;

    this.sourcePositionSelection = new SelectionMap((gp: GenericPosition) => gp.toString());
    this.sourcePositionSelectionHandler = this.initializeSourcePositionSelectionHandler();
    broker.addSourcePositionHandler(this.sourcePositionSelectionHandler);
    this.initializeCode();
  }

  public createViewElement(): HTMLDivElement {
    const sourceContainer = document.createElement("div");
    sourceContainer.classList.add("source-container");
    return sourceContainer;
  }

  private initializeCode(): void {
    const view = this;
    const source = this.source;
    const sourceText = source.sourceText;
    if (!sourceText) return;
    const sourceContainer = view.divNode;
    if (this.codeMode == CodeMode.MainSource) {
      sourceContainer.classList.add("main-source");
    } else {
      sourceContainer.classList.add("inlined-source");
    }
    const codeHeader = document.createElement("div");
    codeHeader.setAttribute("id", this.getCodeHeaderHtmlElementName());
    codeHeader.classList.add("code-header");
    const codeFileFunction = document.createElement("div");
    codeFileFunction.classList.add("code-file-function");
    codeFileFunction.innerHTML = `${source.sourceName}:${source.functionName}`;
    codeHeader.appendChild(codeFileFunction);
    const codeModeDiv = document.createElement("div");
    codeModeDiv.classList.add("code-mode");
    codeModeDiv.innerHTML = this.codeMode;
    codeHeader.appendChild(codeModeDiv);
    const clearDiv = document.createElement("div");
    clearDiv.style.clear = "both";
    codeHeader.appendChild(clearDiv);
    sourceContainer.appendChild(codeHeader);
    const codePre = document.createElement("pre");
    codePre.setAttribute("id", this.getCodeHtmlElementName());
    codePre.classList.add("prettyprint");
    sourceContainer.appendChild(codePre);

    codeHeader.onclick = function myFunction() {
      if (codePre.style.display === "none") {
        codePre.style.display = "block";
      } else {
        codePre.style.display = "none";
      }
    };
    if (sourceText !== "") {
      codePre.classList.add("linenums");
      codePre.textContent = sourceText;

      try {
        // Wrap in try to work when offline.
        PR.prettyPrint(undefined, sourceContainer);
      } catch (e) {
        console.log(e);
      }

      view.divNode.onclick = function (e: MouseEvent) {
        if (e.target instanceof Element && e.target.tagName === "DIV") {
          const targetDiv = e.target as HTMLDivElement;
          if (targetDiv.classList.contains("line-number")) {
            e.stopPropagation();
            view.onSelectLine(Number(targetDiv.dataset.lineNumber), !e.shiftKey);
          }
        } else {
          view.sourcePositionSelectionHandler.clear();
        }
      };

      const base: number = source.startPosition;
      let current = 0;
      const lineListDiv = this.getHtmlCodeLines();
      let newlineAdjust = 0;
      for (let i = 0; i < lineListDiv.length; i++) {
        // Line numbers are not zero-based.
        const lineNumber = i + 1;
        const currentLineElement = lineListDiv[i];
        currentLineElement.id = `li${i}`;
        currentLineElement.dataset.lineNumber = String(lineNumber);
        const spans = currentLineElement.childNodes;
        for (const currentSpan of spans) {
          if (currentSpan instanceof HTMLSpanElement) {
            const pos = base + current;
            const end = pos + currentSpan.textContent.length;
            current += currentSpan.textContent.length;
            this.insertSourcePositions(currentSpan, lineNumber, pos, end, newlineAdjust);
            newlineAdjust = 0;
          }
        }

        this.insertLineNumber(currentLineElement, lineNumber);

        while ((current < sourceText.length) &&
        (sourceText[current] === "\n" || sourceText[current] === "\r")) {
          ++current;
          ++newlineAdjust;
        }
      }
    }
  }

  private initializeSourcePositionSelectionHandler(): SourcePositionSelectionHandler
    & ClearableHandler {
    const view = this;
    const broker = this.broker;
    const sourceResolver = this.sourceResolver;
    return {
      select: function (sourcePositions: Array<SourcePosition>, selected: boolean) {
        const locations = new Array<SourcePosition>();
        for (const sourcePosition of sourcePositions) {
          locations.push(sourcePosition);
          sourceResolver.addInliningPositions(sourcePosition, locations);
        }
        if (locations.length == 0) return;
        view.sourcePositionSelection.select(locations, selected);
        view.updateSelection();
        broker.broadcastSourcePositionSelect(this, locations, selected);
      },
      clear: function () {
        view.sourcePositionSelection.clear();
        view.updateSelection();
        broker.broadcastClear(this);
      },
      brokeredSourcePositionSelect: function (locations: Array<SourcePosition>, selected: boolean) {
        const firstSelect = view.sourcePositionSelection.isEmpty();
        for (const location of locations) {
          const translated = sourceResolver.translateToSourceId(view.source.sourceId, location);
          if (!translated) continue;
          view.sourcePositionSelection.select([translated], selected);
        }
        view.updateSelection(firstSelect);
      },
      brokeredClear: function () {
        view.sourcePositionSelection.clear();
        view.updateSelection();
      },
    };
  }

  private addHtmlElementToSourcePosition(sourcePosition: GenericPosition, element: HTMLElement):
    void {
    const key = sourcePosition.toString();
    if (!this.sourcePositionToHtmlElements.has(key)) {
      this.sourcePositionToHtmlElements.set(key, new Array<HTMLElement>());
    }
    this.sourcePositionToHtmlElements.get(key).push(element);
  }

  private updateSelection(scrollIntoView: boolean = false): void {
    const mkVisible = new ViewElements(this.divNode.parentNode as HTMLElement);
    for (const [sp, els] of this.sourcePositionToHtmlElements.entries()) {
      const isSelected = this.sourcePositionSelection.isKeySelected(sp);
      for (const el of els) {
        mkVisible.consider(el, isSelected);
        el.classList.toggle("selected", isSelected);
      }
    }
    mkVisible.apply(scrollIntoView);
  }

  private getCodeHtmlElementName(): string {
    return `source-pre-${this.source.sourceId}`;
  }

  private getCodeHeaderHtmlElementName(): string {
    return `source-pre-${this.source.sourceId}-header`;
  }

  private getHtmlCodeLines(): NodeListOf<HTMLElement> {
    const orderList = this.divNode.querySelector(`#${this.getCodeHtmlElementName()} ol`);
    return orderList.childNodes as NodeListOf<HTMLElement>;
  }

  private onSelectLine(lineNumber: number, doClear: boolean) {
    if (doClear) {
      this.sourcePositionSelectionHandler.clear();
    }
    const positions = this.sourceResolver.lineToSourcePositions(lineNumber - 1);
    if (positions !== undefined) {
      this.sourcePositionSelectionHandler.select(positions, undefined);
    }
  }

  private onSelectSourcePosition(sourcePosition: SourcePosition, doClear: boolean) {
    if (doClear) {
      this.sourcePositionSelectionHandler.clear();
    }
    this.sourcePositionSelectionHandler.select([sourcePosition], undefined);
  }

  private insertSourcePositions(currentSpan: HTMLSpanElement, lineNumber: number,
                                pos: number, end: number, adjust: number): void {
    const view = this;
    const sps = this.sourceResolver.sourcePositionsInRange(this.source.sourceId, pos - adjust, end);
    let offset = 0;
    for (const sourcePosition of sps) {
      // Internally, line numbers are 0-based so we have to substract 1 from the line number. This
      // path in only taken by non-Wasm code. Wasm code relies on setSourceLineToBytecodePosition.
      this.sourceResolver.addAnyPositionToLine(lineNumber - 1, sourcePosition);
      const textNode = currentSpan.tagName === "SPAN" ? currentSpan.lastChild : currentSpan;
      if (!(textNode instanceof Text)) continue;
      const splitLength = Math.max(0, sourcePosition.scriptOffset - pos - offset);
      offset += splitLength;
      const replacementNode = textNode.splitText(splitLength);
      const span = document.createElement("span");
      span.setAttribute("scriptOffset", sourcePosition.scriptOffset.toString());
      span.classList.add("source-position");
      const marker = document.createElement("span");
      marker.classList.add("marker");
      span.appendChild(marker);
      const inlining = this.sourceResolver.getInliningForPosition(sourcePosition);
      if (inlining && view.showAdditionalInliningPosition) {
        const sourceName = this.sourceResolver.getSourceName(inlining.sourceId);
        const inliningMarker = document.createElement("span");
        inliningMarker.classList.add("inlining-marker");
        inliningMarker.setAttribute("data-descr", `${sourceName} was inlined here`);
        span.appendChild(inliningMarker);
      }
      span.onclick = function (e: MouseEvent) {
        e.stopPropagation();
        view.onSelectSourcePosition(sourcePosition, !e.shiftKey);
      };
      view.addHtmlElementToSourcePosition(sourcePosition, span);
      textNode.parentNode.insertBefore(span, replacementNode);
    }
  }

  private insertLineNumber(lineElement: HTMLElement, lineNumber: number): void {
    const lineNumberElement = document.createElement("div");
    lineNumberElement.classList.add("line-number");
    lineNumberElement.dataset.lineNumber = String(lineNumber);
    lineNumberElement.innerText = String(lineNumber);
    lineElement.insertBefore(lineNumberElement, lineElement.firstChild);
    for (const sourcePosition of this.sourceResolver.lineToSourcePositions(lineNumber - 1)) {
      this.addHtmlElementToSourcePosition(sourcePosition, lineElement);
    }
  }
}
