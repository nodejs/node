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

    document.getElementById("show-hide-source").addEventListener("click", () => {
      this.resizer.resizerLeft.classed("snapped", !this.resizer.resizerLeft.classed("snapped"));
      this.setSourceExpanded(!this.sourceExpand.classList.contains("invisible"));
      this.resizer.updatePanes();
    });
    document.getElementById("show-hide-disassembly").addEventListener("click", () => {
      this.resizer.resizerRight.classed("snapped", !this.resizer.resizerRight.classed("snapped"));
      this.setDisassemblyExpanded(!this.disassemblyExpand.classList.contains("invisible"));
      this.resizer.updatePanes();
    });
  }

  restoreExpandedState(): void {
    this.resizer.resizerLeft.classed("snapped", window.sessionStorage.getItem("expandedState-source") == "false");
    this.resizer.resizerRight.classed("snapped", window.sessionStorage.getItem("expandedState-disassembly") == "false");
    this.setSourceExpanded(this.getLastExpandedState("source", true));
    this.setDisassemblyExpanded(this.getLastExpandedState("disassembly", true));
  }

  getLastExpandedState(type: string, defaultState: boolean): boolean {
    const state = window.sessionStorage.getItem("expandedState-" + type);
    if (state === null) return defaultState;
    return state === 'true';
  }

  sourceUpdate(isSourceExpanded: boolean): void {
    window.sessionStorage.setItem("expandedState-source", `${isSourceExpanded}`);
    this.sourceExpand.classList.toggle("invisible", isSourceExpanded);
    this.sourceCollapse.classList.toggle("invisible", !isSourceExpanded);
  }

  setSourceExpanded(isSourceExpanded: boolean): void {
    this.sourceUpdate(isSourceExpanded);
    this.resizer.updateLeftWidth();
  }

  disassemblyUpdate(isDisassemblyExpanded: boolean): void {
    window.sessionStorage.setItem("expandedState-disassembly", `${isDisassemblyExpanded}`);
    this.disassemblyExpand.classList.toggle("invisible", isDisassemblyExpanded);
    this.disassemblyCollapse.classList.toggle("invisible", !isDisassemblyExpanded);
  }

  setDisassemblyExpanded(isDisassemblyExpanded: boolean): void {
    this.disassemblyUpdate(isDisassemblyExpanded);
    this.resizer.updateRightWidth();
  }
}

export class Resizer {
  snapper: Snapper;
  deadWidth: number;
  left: HTMLElement;
  right: HTMLElement;
  sepLeft: number;
  sepRight: number;
  panesUpdatedCallback: () => void;
  resizerRight: d3.Selection<HTMLDivElement, any, any, any>;
  resizerLeft: d3.Selection<HTMLDivElement, any, any, any>;

  private readonly SOURCE_PANE_DEFAULT_PERCENT = 1 / 4;
  private readonly DISASSEMBLY_PANE_DEFAULT_PERCENT = 3 / 4;

  constructor(panesUpdatedCallback: () => void, deadWidth: number) {
    const resizer = this;
    resizer.panesUpdatedCallback = panesUpdatedCallback;
    resizer.deadWidth = deadWidth;
    resizer.left = document.getElementById(C.SOURCE_PANE_ID);
    resizer.right = document.getElementById(C.GENERATED_PANE_ID);
    resizer.resizerLeft = d3.select('#resizer-left');
    resizer.resizerRight = d3.select('#resizer-right');
    // Set default sizes, if they weren't set.
    if (window.sessionStorage.getItem("source-pane-percent") === null) {
      window.sessionStorage.setItem("source-pane-percent", `${this.SOURCE_PANE_DEFAULT_PERCENT}`);
    }
    if (window.sessionStorage.getItem("disassembly-pane-percent") === null) {
      window.sessionStorage.setItem("disassembly-pane-percent", `${this.DISASSEMBLY_PANE_DEFAULT_PERCENT}`);
    }

    this.updateWidths();

    const dragResizeLeft = d3.drag()
      .on('drag', function () {
        const x = d3.mouse(this.parentElement)[0];
        resizer.sepLeft = Math.min(Math.max(0, x), resizer.sepRight);
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizerLeft.classed("dragged", true);
      })
      .on('end', function () {
        // If the panel is close enough to the left, treat it as if it was pulled all the way to the lefg.
        const x = d3.mouse(this.parentElement)[0];
        if (x <= deadWidth) {
          resizer.sepLeft = 0;
          resizer.updatePanes();
        }
        // Snap if dragged all the way to the left.
        resizer.resizerLeft.classed("snapped", resizer.sepLeft === 0);
        if (!resizer.isLeftSnapped()) {
          window.sessionStorage.setItem("source-pane-percent", `${resizer.sepLeft / document.body.getBoundingClientRect().width}`);
        }
        resizer.snapper.setSourceExpanded(!resizer.isLeftSnapped());
        resizer.resizerLeft.classed("dragged", false);
      });
    resizer.resizerLeft.call(dragResizeLeft);

    const dragResizeRight = d3.drag()
      .on('drag', function () {
        const x = d3.mouse(this.parentElement)[0];
        resizer.sepRight = Math.max(resizer.sepLeft, Math.min(x, document.body.getBoundingClientRect().width));
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizerRight.classed("dragged", true);
      })
      .on('end', function () {
        // If the panel is close enough to the right, treat it as if it was pulled all the way to the right.
        const x = d3.mouse(this.parentElement)[0];
        const clientWidth = document.body.getBoundingClientRect().width;
        if (x >= (clientWidth - deadWidth)) {
          resizer.sepRight = clientWidth - 1;
          resizer.updatePanes();
        }
        // Snap if dragged all the way to the right.
        resizer.resizerRight.classed("snapped", resizer.sepRight >= clientWidth - 1);
        if (!resizer.isRightSnapped()) {
          window.sessionStorage.setItem("disassembly-pane-percent", `${resizer.sepRight / clientWidth}`);
        }
        resizer.snapper.setDisassemblyExpanded(!resizer.isRightSnapped());
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
    return this.resizerLeft.classed("snapped");
  }

  isRightSnapped() {
    return this.resizerRight.classed("snapped");
  }

  updatePanes() {
    this.left.style.width = this.sepLeft + 'px';
    this.resizerLeft.style('left', this.sepLeft + 'px');
    this.right.style.width = (document.body.getBoundingClientRect().width - this.sepRight) + 'px';
    this.resizerRight.style('right', (document.body.getBoundingClientRect().width - this.sepRight - 1) + 'px');

    this.panesUpdatedCallback();
  }

  updateLeftWidth() {
    if (this.isLeftSnapped()) {
      this.sepLeft = 0;
    } else {
      const sepLeft = window.sessionStorage.getItem("source-pane-percent");
      this.sepLeft = document.body.getBoundingClientRect().width * Number.parseFloat(sepLeft);
    }
  }

  updateRightWidth() {
    if (this.isRightSnapped()) {
      this.sepRight = document.body.getBoundingClientRect().width;
    } else {
      const sepRight = window.sessionStorage.getItem("disassembly-pane-percent");
      this.sepRight = document.body.getBoundingClientRect().width * Number.parseFloat(sepRight);
    }
  }

  updateWidths() {
    this.updateLeftWidth();
    this.updateRightWidth();
  }
}
