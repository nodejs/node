// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as d3 from "d3";
import * as C from "../src/constants";

class Snapper {
  resizer: Resizer;
  sourceExpand: HTMLElement;
  sourceCollapse: HTMLElement;
  disassemblyExpand: HTMLElement;
  disassemblyCollapse: HTMLElement;

  constructor(resizer: Resizer) {
    this.resizer = resizer;
    this.sourceExpand = document.getElementById(C.SOURCE_EXPAND_ID);
    this.sourceCollapse = document.getElementById(C.SOURCE_COLLAPSE_ID);
    this.disassemblyExpand = document.getElementById(C.DISASSEMBLY_EXPAND_ID);
    this.disassemblyCollapse = document.getElementById(C.DISASSEMBLY_COLLAPSE_ID);

    document.getElementById("source-collapse").addEventListener("click", () => {
      this.setSourceExpanded(!this.sourceExpand.classList.contains("invisible"));
      this.resizer.updatePanes();
    });
    document.getElementById("disassembly-collapse").addEventListener("click", () => {
      this.setDisassemblyExpanded(!this.disassemblyExpand.classList.contains("invisible"));
      this.resizer.updatePanes();
    });
  }

  restoreExpandedState(): void {
    this.setSourceExpanded(this.getLastExpandedState("source", true));
    this.setDisassemblyExpanded(this.getLastExpandedState("disassembly", false));
  }

  getLastExpandedState(type: string, defaultState: boolean): boolean {
    const state = window.sessionStorage.getItem("expandedState-" + type);
    if (state === null) return defaultState;
    return state === 'true';
  }

  sourceExpandUpdate(newState: boolean): void {
    window.sessionStorage.setItem("expandedState-source", `${newState}`);
    this.sourceExpand.classList.toggle("invisible", newState);
    this.sourceCollapse.classList.toggle("invisible", !newState);
  }

  setSourceExpanded(newState: boolean): void {
    if (this.sourceExpand.classList.contains("invisible") === newState) return;
    const resizer = this.resizer;
    this.sourceExpandUpdate(newState);
    if (newState) {
      resizer.sepLeft = resizer.sepLeftSnap;
      resizer.sepLeftSnap = 0;
    } else {
      resizer.sepLeftSnap = resizer.sepLeft;
      resizer.sepLeft = 0;
    }
  }

  disassemblyExpandUpdate(newState: boolean): void {
    window.sessionStorage.setItem("expandedState-disassembly", `${newState}`);
    this.disassemblyExpand.classList.toggle("invisible", newState);
    this.disassemblyCollapse.classList.toggle("invisible", !newState);
  }

  setDisassemblyExpanded(newState: boolean): void {
    if (this.disassemblyExpand.classList.contains("invisible") === newState) return;
    const resizer = this.resizer;
    this.disassemblyExpandUpdate(newState);
    if (newState) {
      resizer.sepRight = resizer.sepRightSnap;
      resizer.sepRightSnap = resizer.clientWidth;
    } else {
      resizer.sepRightSnap = resizer.sepRight;
      resizer.sepRight = resizer.clientWidth;
    }
  }

  panesUpdated(): void {
    this.sourceExpandUpdate(this.resizer.sepLeft > this.resizer.deadWidth);
    this.disassemblyExpandUpdate(this.resizer.sepRight <
      (this.resizer.clientWidth - this.resizer.deadWidth));
  }
}

export class Resizer {
  snapper: Snapper;
  deadWidth: number;
  clientWidth: number;
  left: HTMLElement;
  right: HTMLElement;
  middle: HTMLElement;
  sepLeft: number;
  sepRight: number;
  sepLeftSnap: number;
  sepRightSnap: number;
  sepWidthOffset: number;
  panesUpdatedCallback: () => void;
  resizerRight: d3.Selection<HTMLDivElement, any, any, any>;
  resizerLeft: d3.Selection<HTMLDivElement, any, any, any>;

  constructor(panesUpdatedCallback: () => void, deadWidth: number) {
    const resizer = this;
    resizer.panesUpdatedCallback = panesUpdatedCallback;
    resizer.deadWidth = deadWidth;
    resizer.left = document.getElementById(C.SOURCE_PANE_ID);
    resizer.middle = document.getElementById(C.INTERMEDIATE_PANE_ID);
    resizer.right = document.getElementById(C.GENERATED_PANE_ID);
    resizer.resizerLeft = d3.select('#resizer-left');
    resizer.resizerRight = d3.select('#resizer-right');
    resizer.sepLeftSnap = 0;
    resizer.sepRightSnap = 0;
    // Offset to prevent resizers from sliding slightly over one another.
    resizer.sepWidthOffset = 7;
    this.updateWidths();

    const dragResizeLeft = d3.drag()
      .on('drag', function () {
        const x = d3.mouse(this.parentElement)[0];
        resizer.sepLeft = Math.min(Math.max(0, x), resizer.sepRight - resizer.sepWidthOffset);
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizerLeft.classed("dragged", true);
        const x = d3.mouse(this.parentElement)[0];
        if (x > deadWidth) {
          resizer.sepLeftSnap = resizer.sepLeft;
        }
      })
      .on('end', function () {
        if (!resizer.isRightSnapped()) {
          window.sessionStorage.setItem("source-pane-width", `${resizer.sepLeft / resizer.clientWidth}`);
        }
        resizer.resizerLeft.classed("dragged", false);
      });
    resizer.resizerLeft.call(dragResizeLeft);

    const dragResizeRight = d3.drag()
      .on('drag', function () {
        const x = d3.mouse(this.parentElement)[0];
        resizer.sepRight = Math.max(resizer.sepLeft + resizer.sepWidthOffset, Math.min(x, resizer.clientWidth));
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizerRight.classed("dragged", true);
        const x = d3.mouse(this.parentElement)[0];
        if (x < (resizer.clientWidth - deadWidth)) {
          resizer.sepRightSnap = resizer.sepRight;
        }
      })
      .on('end', function () {
        if (!resizer.isRightSnapped()) {
          console.log(`disassembly-pane-width ${resizer.sepRight}`);
          window.sessionStorage.setItem("disassembly-pane-width", `${resizer.sepRight / resizer.clientWidth}`);
        }
        resizer.resizerRight.classed("dragged", false);
      });
    resizer.resizerRight.call(dragResizeRight);
    window.onresize = function () {
      resizer.updateWidths();
      resizer.updatePanes();
    };
    resizer.snapper = new Snapper(resizer);
    resizer.snapper.restoreExpandedState();
  }

  isLeftSnapped() {
    return this.sepLeft === 0;
  }

  isRightSnapped() {
    return this.sepRight >= this.clientWidth - 1;
  }

  updatePanes() {
    const leftSnapped = this.isLeftSnapped();
    const rightSnapped = this.isRightSnapped();
    this.resizerLeft.classed("snapped", leftSnapped);
    this.resizerRight.classed("snapped", rightSnapped);
    this.left.style.width = this.sepLeft + 'px';
    this.middle.style.width = (this.sepRight - this.sepLeft) + 'px';
    this.right.style.width = (this.clientWidth - this.sepRight) + 'px';
    this.resizerLeft.style('left', this.sepLeft + 'px');
    this.resizerRight.style('right', (this.clientWidth - this.sepRight - 1) + 'px');

    this.snapper.panesUpdated();
    this.panesUpdatedCallback();
  }

  updateWidths() {
    this.clientWidth = document.body.getBoundingClientRect().width;
    const sepLeft = window.sessionStorage.getItem("source-pane-width");
    this.sepLeft = this.clientWidth * (sepLeft ? Number.parseFloat(sepLeft) : (1 / 3));
    const sepRight = window.sessionStorage.getItem("disassembly-pane-width");
    this.sepRight = this.clientWidth * (sepRight ? Number.parseFloat(sepRight) : (2 / 3));
  }
}
