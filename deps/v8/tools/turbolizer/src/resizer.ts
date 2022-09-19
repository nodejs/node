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
  rangesExpand: HTMLElement;
  rangesCollapse: HTMLElement;

  constructor(resizer: Resizer) {
    this.resizer = resizer;
    this.sourceExpand = document.getElementById(C.SOURCE_EXPAND_ID);
    this.sourceCollapse = document.getElementById(C.SOURCE_COLLAPSE_ID);
    this.disassemblyExpand = document.getElementById(C.DISASSEMBLY_EXPAND_ID);
    this.disassemblyCollapse = document.getElementById(C.DISASSEMBLY_COLLAPSE_ID);
    this.rangesExpand = document.getElementById(C.RANGES_EXPAND_ID);
    this.rangesCollapse = document.getElementById(C.RANGES_COLLAPSE_ID);

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
    document.getElementById("show-hide-ranges").addEventListener("click", () => {
      this.resizer.resizerRanges.classed("snapped", !this.resizer.resizerRanges.classed("snapped"));
      this.setRangesExpanded(!this.rangesExpand.classList.contains("invisible"));
      this.resizer.updatePanes();
    });
  }

  restoreExpandedState(): void {
    this.resizer.resizerLeft.classed("snapped", window.sessionStorage.getItem("expandedState-source") == "false");
    this.resizer.resizerRight.classed("snapped", window.sessionStorage.getItem("expandedState-disassembly") == "false");
    this.resizer.resizerRanges.classed("snapped", window.sessionStorage.getItem("expandedState-ranges") == "false");
    this.setSourceExpanded(this.getLastExpandedState("source", true));
    this.setDisassemblyExpanded(this.getLastExpandedState("disassembly", true));
    this.setRangesExpanded(this.getLastExpandedState("ranges", true));
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
    document.getElementById("show-hide-ranges").style.marginLeft = isSourceExpanded ? null : "40px";
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

  rangesUpdate(isRangesExpanded: boolean): void {
    window.sessionStorage.setItem("expandedState-ranges", `${isRangesExpanded}`);
    this.rangesExpand.classList.toggle("invisible", isRangesExpanded);
    this.rangesCollapse.classList.toggle("invisible", !isRangesExpanded);
  }

  setRangesExpanded(isRangesExpanded: boolean): void {
    this.rangesUpdate(isRangesExpanded);
    this.resizer.updateRanges();
  }
}

export class Resizer {
  snapper: Snapper;
  deadWidth: number;
  deadHeight: number;
  left: HTMLElement;
  right: HTMLElement;
  ranges: HTMLElement;
  middle: HTMLElement;
  sepLeft: number;
  sepRight: number;
  sepRangesHeight: number;
  panesUpdatedCallback: () => void;
  resizerRight: d3.Selection<HTMLDivElement, any, any, any>;
  resizerLeft: d3.Selection<HTMLDivElement, any, any, any>;
  resizerRanges: d3.Selection<HTMLDivElement, any, any, any>;

  private readonly SOURCE_PANE_DEFAULT_PERCENT = 1 / 4;
  private readonly DISASSEMBLY_PANE_DEFAULT_PERCENT = 3 / 4;
  private readonly RANGES_PANE_HEIGHT_DEFAULT_PERCENT = 3 / 4;
  private readonly RESIZER_RANGES_HEIGHT_BUFFER_PERCENTAGE = 5;
  private readonly RESIZER_SIZE = document.getElementById("resizer-ranges").offsetHeight;

  constructor(panesUpdatedCallback: () => void, deadWidth: number, deadHeight: number) {
    const resizer = this;
    resizer.panesUpdatedCallback = panesUpdatedCallback;
    resizer.deadWidth = deadWidth;
    resizer.deadHeight = deadHeight;
    resizer.left = document.getElementById(C.SOURCE_PANE_ID);
    resizer.right = document.getElementById(C.GENERATED_PANE_ID);
    resizer.ranges = document.getElementById(C.RANGES_PANE_ID);
    resizer.middle = document.getElementById("middle");
    resizer.resizerLeft = d3.select('#resizer-left');
    resizer.resizerRight = d3.select('#resizer-right');
    resizer.resizerRanges = d3.select('#resizer-ranges');
    // Set default sizes, if they weren't set.
    if (window.sessionStorage.getItem("source-pane-percent") === null) {
      window.sessionStorage.setItem("source-pane-percent", `${this.SOURCE_PANE_DEFAULT_PERCENT}`);
    }
    if (window.sessionStorage.getItem("disassembly-pane-percent") === null) {
      window.sessionStorage.setItem("disassembly-pane-percent", `${this.DISASSEMBLY_PANE_DEFAULT_PERCENT}`);
    }
    if (window.sessionStorage.getItem("ranges-pane-height-percent") === null) {
      window.sessionStorage.setItem("ranges-pane-height-percent", `${this.RANGES_PANE_HEIGHT_DEFAULT_PERCENT}`);
    }

    this.updateSizes();

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

    const dragResizeRanges = d3.drag()
      .on('drag', function () {
        const y = d3.mouse(this.parentElement)[1];
        resizer.sepRangesHeight = Math.max(100, Math.min(y, window.innerHeight) - resizer.RESIZER_RANGES_HEIGHT_BUFFER_PERCENTAGE);
        resizer.updatePanes();
      })
      .on('start', function () {
        resizer.resizerRanges.classed("dragged", true);
      })
      .on('end', function () {
        // If the panel is close enough to the bottom, treat it as if it was pulled all the way to the bottom.
        const y = d3.mouse(this.parentElement)[1];
        if (y >= (window.innerHeight - deadHeight)) {
          resizer.sepRangesHeight = window.innerHeight;
          resizer.updatePanes();
        }
        // Snap if dragged all the way to the bottom.
        resizer.resizerRanges.classed("snapped", resizer.sepRangesHeight >= window.innerHeight - 1);
        if (!resizer.isRangesSnapped()) {
          window.sessionStorage.setItem("ranges-pane-height-percent", `${resizer.sepRangesHeight / window.innerHeight}`);
        }
        resizer.snapper.setRangesExpanded(!resizer.isRangesSnapped());
        resizer.resizerRanges.classed("dragged", false);
      });
    resizer.resizerRanges.call(dragResizeRanges);

    window.onresize = function () {
      resizer.updateSizes();
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

  isRangesSnapped() {
    return this.resizerRanges.classed("snapped");
  }

  updateRangesPane() {
    const clientHeight = window.innerHeight;
    const rangesIsHidden = this.ranges.style.visibility == "hidden";
    let resizerSize = this.RESIZER_SIZE;
    if (rangesIsHidden) {
      resizerSize = 0;
      this.sepRangesHeight = clientHeight;
    }

    const rangeHeight = clientHeight - this.sepRangesHeight;
    this.ranges.style.height = rangeHeight + 'px';
    const panelWidth = this.sepRight - this.sepLeft - (2 * resizerSize);
    this.ranges.style.width = panelWidth + 'px';
    const multiview = document.getElementById("multiview");
    if (multiview && multiview.style) {
        multiview.style.height = (this.sepRangesHeight - resizerSize) + 'px';
        multiview.style.width = panelWidth + 'px';
    }

    // Resize the range grid and labels.
    const rangeGrid = (this.ranges.getElementsByClassName("range-grid")[0] as HTMLElement);
    if (rangeGrid) {
      const yAxis = (this.ranges.getElementsByClassName("range-y-axis")[0] as HTMLElement);
      const rangeHeader = (this.ranges.getElementsByClassName("range-header")[0] as HTMLElement);

      const gridWidth = panelWidth - yAxis.clientWidth;
      rangeGrid.style.width = Math.floor(gridWidth - 1) + 'px';
      // Take live ranges' right scrollbar into account.
      rangeHeader.style.width = (gridWidth - rangeGrid.offsetWidth + rangeGrid.clientWidth - 1) + 'px';
      // Set resizer to horizontal.
      this.resizerRanges.style('width', panelWidth + 'px');

      const rangeTitle = (this.ranges.getElementsByClassName("range-title-div")[0] as HTMLElement);
      const rangeHeaderLabel = (this.ranges.getElementsByClassName("range-header-label-x")[0] as HTMLElement);
      const gridHeight = rangeHeight - rangeHeader.clientHeight - rangeTitle.clientHeight - rangeHeaderLabel.clientHeight;
      rangeGrid.style.height = gridHeight + 'px';
      // Take live ranges' bottom scrollbar into account.
      yAxis.style.height = (gridHeight - rangeGrid.offsetHeight + rangeGrid.clientHeight) + 'px';
    }
    this.resizerRanges.style('ranges', this.ranges.style.height);
  }

  updatePanes() {
    this.left.style.width = this.sepLeft + 'px';
    this.resizerLeft.style('left', this.sepLeft + 'px');
    this.right.style.width = (document.body.getBoundingClientRect().width - this.sepRight) + 'px';
    this.resizerRight.style('right', (document.body.getBoundingClientRect().width - this.sepRight - 1) + 'px');
    this.updateRangesPane();
    this.panesUpdatedCallback();
  }

  updateRanges() {
    if (this.isRangesSnapped()) {
      this.sepRangesHeight = window.innerHeight;
    } else {
      const sepRangesHeight = window.sessionStorage.getItem("ranges-pane-height-percent");
      this.sepRangesHeight = window.innerHeight * Number.parseFloat(sepRangesHeight);
    }
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

  updateSizes() {
    this.updateLeftWidth();
    this.updateRightWidth();
    this.updateRanges();
  }
}
