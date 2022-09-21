// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../common/constants";
import { createElement, getNumericCssValue, storageGetItem, storageSetItem } from "../common/util";
import { SequenceView } from "./sequence-view";
import { Interval } from "../interval";
import { ChildRange, Range, SequenceBlock } from "../phases/sequence-phase";

// This class holds references to the HTMLElements that represent each cell.
class Grid {
  elements: Array<Array<HTMLElement>>;

  constructor() {
    this.elements = new Array<Array<HTMLElement>>();
  }

  public setRow(row: number, elementsRow: Array<HTMLElement>): void {
    this.elements[row] = elementsRow;
  }

  public getCell(row: number, column: number): HTMLElement {
    return this.elements[row][column];
  }

  public getInterval(row: number, column: number): HTMLElement {
    // The cell is within an inner wrapper div which is within the interval div.
    return this.getCell(row, column).parentElement.parentElement;
  }
}

// This class is used as a wrapper to hide the switch between the
// two different Grid objects used, one for each phase,
// before and after register allocation.
class GridAccessor {
  sequenceView: SequenceView;
  grids: Map<number, Grid>;

  constructor(sequenceView: SequenceView) {
    this.sequenceView = sequenceView;
    this.grids = new Map<number, Grid>();
  }

  public getAnyGrid(): Grid {
    return this.grids.values().next().value;
  }

  public hasGrid(): boolean {
    return this.grids.has(this.sequenceView.currentPhaseIndex);
  }

  public addGrid(grid: Grid): void {
    if (this.hasGrid()) console.warn("Overwriting existing Grid.");
    this.grids.set(this.sequenceView.currentPhaseIndex, grid);
  }

  public getCell(row: number, column: number): HTMLElement {
    return this.currentGrid().getCell(row, column);
  }

  public getInterval(row: number, column: number): HTMLElement {
    return this.currentGrid().getInterval(row, column);
  }

  private currentGrid(): Grid {
    return this.grids.get(this.sequenceView.currentPhaseIndex);
  }
}

// This class is used as a wrapper to access the interval HTMLElements
class IntervalElementsAccessor {
  sequenceView: SequenceView;
  map: Map<number, Array<HTMLElement>>;

  constructor(sequenceView: SequenceView) {
    this.sequenceView = sequenceView;
    this.map = new Map<number, Array<HTMLElement>>();
  }

  public addInterval(interval: HTMLElement): void {
    this.currentIntervals().push(interval);
  }

  public forEachInterval(callback: (phase: number, interval: HTMLElement) => void) {
    for (const phase of this.map.keys()) {
      for (const interval of this.map.get(phase)) {
        callback(phase, interval);
      }
    }
  }

  private currentIntervals(): Array<HTMLElement> {
    const intervals = this.map.get(this.sequenceView.currentPhaseIndex);
    if (intervals === undefined) {
      this.map.set(this.sequenceView.currentPhaseIndex, new Array<HTMLElement>());
      return this.currentIntervals();
    }
    return intervals;
  }
}

// A number of css variables regarding dimensions of HTMLElements are required by RangeView.
class CSSVariables {
  positionWidth: number;
  blockBorderWidth: number;

  constructor() {
    this.positionWidth = getNumericCssValue("--range-position-width");
    this.blockBorderWidth = getNumericCssValue("--range-block-border");
  }
}

class UserSettingsObject {
  value: boolean;
  resetFunction: (param: boolean) => void;

  constructor(value: boolean, resetFunction: (param: boolean) => void) {
    this.value = value;
    this.resetFunction = resetFunction;
  }
}

// Manages the user's setting options regarding how the grid is displayed.
class UserSettings {
  settings: Map<string, UserSettingsObject>;

  constructor() {
    this.settings = new Map<string, UserSettingsObject>();
  }

  public addSetting(settingName: string, value: boolean, resetFunction: (param: boolean) => void) {
    this.settings.set(settingName, new UserSettingsObject(value, resetFunction));
  }

  public getToggleElement(settingName: string, settingLabel: string): HTMLElement {
    const toggleEl = createElement("label", "range-toggle-setting", settingLabel);
    const toggleInput = createElement("input", "range-toggle-setting") as HTMLInputElement;
    toggleInput.id = `range-toggle-${settingName}`;
    toggleInput.setAttribute("type", "checkbox");
    toggleInput.oninput = () => {
      toggleInput.disabled = true;
      this.set(settingName, toggleInput.checked);
      this.reset(settingName);
      toggleInput.disabled = false;
    };
    toggleEl.insertBefore(toggleInput, toggleEl.firstChild);
    return toggleEl;
  }

  public reset(settingName: string): void {
    const settingObject = this.settings.get(settingName);
    storageSetItem(this.getSettingKey(settingName), settingObject.value);
    settingObject.resetFunction(settingObject.value);
  }

  public get(settingName: string): boolean {
    return this.settings.get(settingName).value;
  }

  public set(settingName: string, value: boolean): void {
    this.settings.get(settingName).value = value;
  }

  public resetFromSessionStorage(): void {
    for (const [settingName, setting] of this.settings.entries()) {
      const storedValue = storageGetItem(this.getSettingKey(settingName));
      if (storedValue === undefined) continue;
      this.set(settingName, storedValue);
      if (storedValue) {
        const toggle = document.getElementById(`range-toggle-${settingName}`) as HTMLInputElement;
        if (!toggle.checked) {
          toggle.checked = storedValue;
          setting.resetFunction(storedValue);
        }
      }
    }
  }

  private getSettingKey(settingName: string): string {
    return `${C.SESSION_STORAGE_PREFIX}${settingName}`;
  }
}

// Store the required data from the blocks JSON.
class BlocksData {
  blockBorders: Set<number>;
  blockInstructionCountMap: Map<number, number>;

  constructor(blocks: Array<SequenceBlock>) {
    this.blockBorders = new Set<number>();
    this.blockInstructionCountMap = new Map<number, number>();
    for (const block of blocks) {
      this.blockInstructionCountMap.set(block.id, block.instructions.length);
      const maxInstructionInBlock = block.instructions[block.instructions.length - 1].id;
      this.blockBorders.add(maxInstructionInBlock);
    }
  }

  public isInstructionBorder(position: number): boolean {
    return ((position + 1) % C.POSITIONS_PER_INSTRUCTION) == 0;
  }

  public isBlockBorder(position: number): boolean {
    const border = Math.floor(position / C.POSITIONS_PER_INSTRUCTION);
    return this.isInstructionBorder(position) && this.blockBorders.has(border);
  }
}

class Divs {
  // Already existing.
  container: HTMLElement;
  resizerBar: HTMLElement;
  snapper: HTMLElement;

  // Created by constructor.
  content: HTMLElement;
  // showOnLoad contains all content that may change depending on the JSON.
  showOnLoad: HTMLElement;
  xAxisLabel: HTMLElement;
  yAxisLabel: HTMLElement;
  registerHeaders: HTMLElement;
  registers: HTMLElement;

  // Assigned from RangeView.
  wholeHeader: HTMLElement;
  positionHeaders: HTMLElement;
  yAxis: HTMLElement;
  grid: HTMLElement;

  constructor(userSettings: UserSettings) {
    this.container = document.getElementById(C.RANGES_PANE_ID);
    this.resizerBar = document.getElementById(C.RESIZER_RANGES_ID);
    this.snapper = document.getElementById(C.SHOW_HIDE_RANGES_ID);

    this.content = document.createElement("div");
    this.content.appendChild(this.elementForTitle(userSettings));

    this.showOnLoad = document.createElement("div");
    this.showOnLoad.style.visibility = "hidden";
    this.content.appendChild(this.showOnLoad);

    this.xAxisLabel = createElement("div", "range-header-label-x");
    this.xAxisLabel.innerText = "Blocks, Instructions, and Positions";
    this.showOnLoad.appendChild(this.xAxisLabel);
    this.yAxisLabel = createElement("div", "range-header-label-y");
    this.yAxisLabel.innerText = "Registers";
    this.showOnLoad.appendChild(this.yAxisLabel);

    this.registerHeaders = createElement("div", "range-register-labels");
    this.registers = createElement("div", "range-registers");
    this.registerHeaders.appendChild(this.registers);
  }

  public elementForTitle(userSettings: UserSettings): HTMLElement {
    const titleEl = createElement("div", "range-title-div");
    const titleBar = createElement("div", "range-title");
    titleBar.appendChild(createElement("div", "", "Live Ranges"));
    const titleHelp = createElement("div", "range-title-help", "?");
    titleHelp.title = "Each row represents a single TopLevelLiveRange (or two if deferred exists)."
      + "\nEach interval belongs to a LiveRange contained within that row's TopLevelLiveRange."
      + "\nAn interval is identified by i, the index of the LiveRange within the TopLevelLiveRange,"
      + "\nand j, the index of the interval within the LiveRange, to give i:j.";
    titleEl.appendChild(titleBar);
    titleEl.appendChild(titleHelp);
    titleEl.appendChild(userSettings.getToggleElement("landscapeMode", "Landscape Mode"));
    return titleEl;
  }
}

class RowConstructor {
  view: RangeView;

  constructor(view: RangeView) {
    this.view = view;
  }

  // Constructs the row of HTMLElements for grid while providing a callback for each position
  // depending on whether that position is the start of an interval or not.
  // RangePair is used to allow the two fixed register live ranges of normal and deferred to be
  // easily combined into a single row.
  construct(grid: Grid, row: number, registerIndex: number, ranges: [Range, Range],
            getElementForEmptyPosition: (position: number) => HTMLElement,
            callbackForInterval: (position: number, interval: HTMLElement) => void): void {
    const positions = new Array<HTMLElement>(this.view.numPositions);
    // Construct all of the new intervals.
    const intervalMap = this.elementsForIntervals(registerIndex, ranges);
    for (let position = 0; position < this.view.numPositions; ++position) {
      const interval = intervalMap.get(position);
      if (interval === undefined) {
        positions[position] = getElementForEmptyPosition(position);
      } else {
        callbackForInterval(position, interval);
        this.view.intervalsAccessor.addInterval(interval);
        const intervalPositionElements = this.getPositionElementsFromInterval(interval);
        for (let j = 0; j < intervalPositionElements.length; ++j) {
          // Point positionsArray to the new elements.
          positions[position + j] = (intervalPositionElements[j] as HTMLElement);
        }
        position += intervalPositionElements.length - 1;
      }
    }

    grid.setRow(row, positions);

    for (const range of ranges) {
      if (!range) continue;
      this.setUses(grid, row, range);
    }
  }

  private getPositionElementsFromInterval(interval: HTMLElement): HTMLCollection {
    return interval.children[1].children;
  }

  // This is the main function used to build new intervals.
  // Returns a map of LifeTimePositions to intervals.
  private elementsForIntervals(registerIndex: number, ranges: [Range, Range]):
    Map<number, HTMLElement> {
    const intervalMap = new Map<number, HTMLElement>();
    for (const range of ranges) {
      if (!range) continue;
      for (const childRange of range.childRanges) {
        const tooltip = childRange.getTooltip(registerIndex);
        for (const [index, intervalNums] of childRange.intervals.entries()) {
          const interval = new Interval(intervalNums);
          const intervalEl = this.elementForInterval(childRange, interval, tooltip,
            index, range.isDeferred);
          intervalMap.set(interval.start, intervalEl);
        }
      }
    }
    return intervalMap;
  }

  private elementForInterval(childRange: ChildRange, interval: Interval,
                             tooltip: string, index: number, isDeferred: boolean): HTMLElement {
    const intervalEl = createElement("div", "range-interval");
    const title = `${childRange.id}:${index} ${tooltip}`;
    intervalEl.setAttribute("title", isDeferred ? `deferred: ${title}` : title);
    this.setIntervalColor(intervalEl, tooltip);

    const intervalInnerWrapper = createElement("div", "range-interval-wrapper");
    intervalEl.style.gridColumn = `${(interval.start + 1)} / ${(interval.end + 1)}`;
    intervalInnerWrapper.style.gridTemplateColumns = `repeat(${(interval.end - interval.start)}`
      + `,calc(${this.view.cssVariables.positionWidth}ch + `
      + `${this.view.cssVariables.blockBorderWidth}px)`;

    const intervalTextEl = this.elementForIntervalString(tooltip, interval.end - interval.start);
    intervalEl.appendChild(intervalTextEl);

    for (let i = interval.start; i < interval.end; ++i) {
      const classes = "range-position range-interval-position range-empty" +
        (this.view.blocksData.isBlockBorder(i) ? " range-block-border"
          : this.view.blocksData.isInstructionBorder(i) ? " range-instr-border" : "");

      const positionEl = createElement("div", classes, "_");
      positionEl.style.gridColumn = String(i - interval.start + 1);
      intervalInnerWrapper.appendChild(positionEl);
    }

    intervalEl.appendChild(intervalInnerWrapper);
    return intervalEl;
  }

  private setIntervalColor(interval: HTMLElement, tooltip: string): void {
    if (tooltip.includes(C.INTERVAL_TEXT_FOR_NONE)) return;
    if (tooltip.includes(`${C.INTERVAL_TEXT_FOR_STACK}-`)) {
      interval.style.backgroundColor = "rgb(250, 158, 168)";
    } else if (tooltip.includes(C.INTERVAL_TEXT_FOR_STACK)) {
      interval.style.backgroundColor = "rgb(250, 158, 100)";
    } else if (tooltip.includes(C.INTERVAL_TEXT_FOR_CONST)) {
      interval.style.backgroundColor = "rgb(153, 158, 230)";
    } else {
      interval.style.backgroundColor = "rgb(153, 220, 168)";
    }
  }

  private elementForIntervalString(tooltip: string, numCells: number): HTMLElement {
    const spanEl = createElement("span", "range-interval-text");
    this.setIntervalString(spanEl, tooltip, numCells);
    return spanEl;
  }

  // Each interval displays a string of information about it.
  private setIntervalString(spanEl: HTMLElement, tooltip: string, numCells: number): void {
    const spacePerCell = this.view.cssVariables.positionWidth;
    // One character space is removed to accommodate for padding.
    const spaceAvailable = (numCells * spacePerCell) - 0.5;
    let intervalStr = tooltip;
    const length = tooltip.length;
    spanEl.style.width = null;
    let paddingLeft = null;
    // Add padding if possible
    if (length <= spaceAvailable) {
      paddingLeft = (length == spaceAvailable) ? "0.5ch" : "1ch";
    } else {
      intervalStr = "";
    }
    spanEl.style.paddingTop = null;
    spanEl.style.paddingLeft = paddingLeft;
    spanEl.innerHTML = intervalStr;
  }

  private setUses(grid: Grid, row: number, range: Range): void {
    for (const liveRange of range.childRanges) {
      if (!liveRange.uses) continue;
      for (const use of liveRange.uses) {
        grid.getCell(row, use).classList.toggle("range-use", true);
      }
    }
  }
}

class RangeViewConstructor {
  view: RangeView;
  gridTemplateColumns: string;
  grid: Grid;
  // Group the rows in divs to make hiding/showing divs more efficient.
  currentGroup: HTMLElement;
  currentPlaceholderGroup: HTMLElement;

  constructor(rangeView: RangeView) {
    this.view = rangeView;
  }

  public construct(): void {
    this.gridTemplateColumns = `repeat(${this.view.numPositions}`
      + `,calc(${this.view.cssVariables.positionWidth}ch + `
      + `${this.view.cssVariables.blockBorderWidth}px)`;
    this.grid = new Grid();
    this.view.gridAccessor.addGrid(this.grid);

    this.view.divs.wholeHeader = this.elementForHeader();
    this.view.divs.showOnLoad.appendChild(this.view.divs.wholeHeader);

    const gridContainer = document.createElement("div");
    this.view.divs.grid = this.elementForGrid();
    this.view.divs.yAxis = createElement("div", "range-y-axis");
    this.view.divs.yAxis.appendChild(this.view.divs.registerHeaders);
    this.view.divs.yAxis.onscroll = () => {
      this.view.scrollHandler.syncScroll(ToSync.TOP, this.view.divs.yAxis, this.view.divs.grid);
      this.view.scrollHandler.saveScroll();
    };
    gridContainer.appendChild(this.view.divs.yAxis);
    gridContainer.appendChild(this.view.divs.grid);
    this.view.divs.showOnLoad.appendChild(gridContainer);

    this.resetGroups();
    this.addFixedRanges(this.addVirtualRanges(0));
  }

  // The following three functions are for constructing the groups which the rows are contained
  // within and which make up the grid. This is so as to allow groups of rows to easily be displayed
  // and hidden for performance reasons. As rows are constructed, they are added to the currentGroup
  // div. Each row in currentGroup is matched with an equivalent placeholder row in
  // currentPlaceholderGroup that will be shown when currentGroup is hidden so as to maintain the
  // dimensions and scroll positions of the grid.

  private resetGroups(): void {
    this.currentGroup = createElement("div", "range-positions-group range-hidden");
    this.currentPlaceholderGroup = createElement("div", "range-positions-group");
  }

  private appendGroupsToGrid(): void {
    this.view.divs.grid.appendChild(this.currentPlaceholderGroup);
    this.view.divs.grid.appendChild(this.currentGroup);
  }

  private addRowToGroup(row: number, rowEl: HTMLElement): void {
    this.currentGroup.appendChild(rowEl);
    this.currentPlaceholderGroup.appendChild(
      createElement("div", "range-positions range-positions-placeholder", "_"));

    if ((row + 1) % C.ROW_GROUP_SIZE == 0) {
      this.appendGroupsToGrid();
      this.resetGroups();
    }
  }

  private addVirtualRanges(row: number): number {
    const source = this.view.sequenceView.sequence.registerAllocation;
    for (const [registerIndex, range] of source.liveRanges.entries()) {
      if (!range) continue;
      const registerName = this.virtualRegisterName(registerIndex);
      const registerEl = this.elementForVirtualRegister(registerName);
      this.addRowToGroup(row, this.elementForRow(row, registerIndex, [range, undefined]));
      this.view.divs.registers.appendChild(registerEl);
      ++row;
    }
    return row;
  }

  private virtualRegisterName(registerIndex: number): string {
    return `v${registerIndex}`;
  }

  private addFixedRanges(row: number): void {
    row = this.view.sequenceView.sequence.registerAllocation.forEachFixedRange(row,
      (registerIndex: number, row: number, registerName: string, ranges: [Range, Range]) => {
        const registerEl = this.elementForFixedRegister(registerName);
        this.addRowToGroup(row, this.elementForRow(row, registerIndex, ranges));
        this.view.divs.registers.appendChild(registerEl);
      });

    if (row % C.ROW_GROUP_SIZE != 0) {
      this.appendGroupsToGrid();
    }
  }

  // Each row of positions and intervals associated with a register is contained in a single
  // HTMLElement. RangePair is used to allow the two fixed register live ranges of normal and
  // deferred to be easily combined into a single row.
  private elementForRow(row: number, registerIndex: number, ranges: [Range, Range]): HTMLElement {
    const rowEl = createElement("div", "range-positions");
    rowEl.style.gridTemplateColumns = this.gridTemplateColumns;

    const getElementForEmptyPosition = (position: number) => {
      const blockBorder = this.view.blocksData.isBlockBorder(position);
      const classes = "range-position range-empty " + (blockBorder
        ? "range-block-border" : this.view.blocksData.isInstructionBorder(position)
          ? "range-instr-border" : "range-position-border");

      const positionEl = createElement("div", classes, "_");
      positionEl.style.gridColumn = String(position + 1);
      rowEl.appendChild(positionEl);
      return positionEl;
    };

    const callbackForInterval = (_, interval: HTMLElement) => {
      rowEl.appendChild(interval);
    };

    this.view.rowConstructor.construct(this.grid, row, registerIndex, ranges,
      getElementForEmptyPosition, callbackForInterval);

    return rowEl;
  }

  private elementForVirtualRegister(registerName: string): HTMLElement {
    const regEl = createElement("div", "range-reg", registerName);
    regEl.setAttribute("title", registerName);
    return regEl;
  }

  private elementForFixedRegister(registerName: string): HTMLElement {
    let text = registerName;
    const span = "".padEnd(C.FIXED_REGISTER_LABEL_WIDTH - text.length, "_");
    text = "HW - <span class='range-transparent'>" + span + "</span>" + text;
    const regEl = createElement("div", "range-reg");
    regEl.innerHTML = text;
    regEl.setAttribute("title", registerName);
    return regEl;
  }

  // The header element contains the three headers for the LifeTimePosition axis.
  private elementForHeader(): HTMLElement {
    const headerEl = createElement("div", "range-header");
    this.view.divs.positionHeaders = createElement("div", "range-position-labels");

    this.view.divs.positionHeaders.appendChild(this.elementForBlockHeader());
    this.view.divs.positionHeaders.appendChild(this.elementForInstructionHeader());
    this.view.divs.positionHeaders.appendChild(this.elementForPositionHeader());

    headerEl.appendChild(this.view.divs.positionHeaders);
    headerEl.onscroll = () => {
      this.view.scrollHandler.syncScroll(ToSync.LEFT,
        this.view.divs.wholeHeader, this.view.divs.grid);
      this.view.scrollHandler.saveScroll();
    };

    return headerEl;
  }

  // The LifeTimePosition axis shows three headers, for positions, instructions, and blocks.

  private elementForBlockHeader(): HTMLElement {
    const headerEl = createElement("div", "range-block-ids");
    headerEl.style.gridTemplateColumns = this.gridTemplateColumns;

    let blockIndex = 0;
    for (let i = 0; i < this.view.sequenceView.numInstructions;) {
      const instrCount = this.view.blocksData.blockInstructionCountMap.get(blockIndex);
      headerEl.appendChild(this.elementForBlockIndex(blockIndex, i, instrCount));
      ++blockIndex;
      i += instrCount;
    }

    return headerEl;
  }

  private elementForBlockIndex(index: number, firstInstruction: number, instrCount: number):
    HTMLElement {
    const str = `B${index}`;
    const element =
      createElement("div", "range-block-id range-header-element range-block-border", str);
    element.setAttribute("title", str);
    const firstGridCol = (firstInstruction * C.POSITIONS_PER_INSTRUCTION) + 1;
    const lastGridCol = firstGridCol + (instrCount * C.POSITIONS_PER_INSTRUCTION);
    element.style.gridColumn = `${firstGridCol} / ${lastGridCol}`;
    return element;
  }

  private elementForInstructionHeader(): HTMLElement {
    const headerEl = createElement("div", "range-instruction-ids");
    headerEl.style.gridTemplateColumns = this.gridTemplateColumns;

    for (let i = 0; i < this.view.sequenceView.numInstructions; ++i) {
      headerEl.appendChild(this.elementForInstructionIndex(i));
    }

    return headerEl;
  }

  private elementForInstructionIndex(index: number): HTMLElement {
    const isBlockBorder = this.view.blocksData.blockBorders.has(index);
    const classes = "range-instruction-id range-header-element "
      + (isBlockBorder ? "range-block-border" : "range-instr-border");

    const element = createElement("div", classes, String(index));
    element.setAttribute("title", String(index));
    const firstGridCol = (index * C.POSITIONS_PER_INSTRUCTION) + 1;
    element.style.gridColumn = `${firstGridCol} / ${(firstGridCol + C.POSITIONS_PER_INSTRUCTION)}`;
    return element;
  }

  private elementForPositionHeader(): HTMLElement {
    const headerEl = createElement("div", "range-positions range-positions-header");
    headerEl.style.gridTemplateColumns = this.gridTemplateColumns;

    for (let i = 0; i < this.view.numPositions; ++i) {
      headerEl.appendChild(this.elementForPositionIndex(i));
    }

    return headerEl;
  }

  private elementForPositionIndex(index: number): HTMLElement {
    const isBlockBorder = this.view.blocksData.isBlockBorder(index);
    const classes = "range-position range-header-element " +
      (isBlockBorder ? "range-block-border"
        : this.view.blocksData.isInstructionBorder(index) ? "range-instr-border"
          : "range-position-border");

    const element = createElement("div", classes, "" + index);
    element.setAttribute("title", "" + index);
    return element;
  }

  private elementForGrid(): HTMLElement {
    const gridEl = createElement("div", "range-grid");
    gridEl.onscroll = () => {
      this.view.scrollHandler.syncScroll(ToSync.TOP, this.view.divs.grid, this.view.divs.yAxis);
      this.view.scrollHandler.syncScroll(ToSync.LEFT,
        this.view.divs.grid, this.view.divs.wholeHeader);
      this.view.scrollHandler.saveScroll();
    };
    return gridEl;
  }
}

// Handles the work required when the phase is changed.
// Between before and after register allocation for example.
class PhaseChangeHandler {
  view: RangeView;

  constructor(view: RangeView) {
    this.view = view;
  }

  // Called when the phase view is switched between before and after register allocation.
  public phaseChange(): void {
    if (!this.view.gridAccessor.hasGrid()) {
      // If this phase view has not been seen yet then the intervals need to be constructed.
      this.addNewIntervals();
    }
    // Show all intervals pertaining to the current phase view.
    this.view.intervalsAccessor.forEachInterval((phase, interval) => {
      interval.classList.toggle("range-hidden", phase != this.view.sequenceView.currentPhaseIndex);
    });
  }

  private addNewIntervals(): void {
    // All Grids should point to the same HTMLElement for empty cells in the grid,
    // so as to avoid duplication. The current Grid is used to retrieve these elements.
    const currentGrid = this.view.gridAccessor.getAnyGrid();
    const newGrid = new Grid();
    this.view.gridAccessor.addGrid(newGrid);
    const source = this.view.sequenceView.sequence.registerAllocation;

    let row = 0;
    for (const [registerIndex, range] of source.liveRanges.entries()) {
      if (!range) continue;
      this.addnewIntervalsInRange(currentGrid, newGrid, row, registerIndex, [range, undefined]);
      ++row;
    }

    this.view.sequenceView.sequence.registerAllocation.forEachFixedRange(row,
      (registerIndex, row, _, ranges) => {
        this.addnewIntervalsInRange(currentGrid, newGrid, row, registerIndex, ranges);
      });
  }

  private addnewIntervalsInRange(currentGrid: Grid, newGrid: Grid, row: number,
                                 registerIndex: number, ranges: [Range, Range]): void {
    const numReplacements = new Map<HTMLElement, number>();

    const getElementForEmptyPosition = (position: number) => {
      return currentGrid.getCell(row, position);
    };

    // Inserts new interval beside existing intervals.
    const callbackForInterval = (position: number, interval: HTMLElement) => {
      // Overlapping intervals are placed beside each other and the relevant ones displayed.
      let currentInterval = currentGrid.getInterval(row, position);
      // The number of intervals already inserted is tracked so that the inserted intervals
      // are ordered correctly.
      const intervalsAlreadyInserted = numReplacements.get(currentInterval);
      numReplacements.set(currentInterval, intervalsAlreadyInserted
        ? intervalsAlreadyInserted + 1 : 1);

      if (intervalsAlreadyInserted) {
        for (let j = 0; j < intervalsAlreadyInserted; ++j) {
          currentInterval = (currentInterval.nextElementSibling as HTMLElement);
        }
      }

      interval.classList.add("range-hidden");
      currentInterval.insertAdjacentElement("afterend", interval);
    };

    this.view.rowConstructor.construct(newGrid, row, registerIndex, ranges,
      getElementForEmptyPosition, callbackForInterval);
  }
}

enum ToSync {
  LEFT,
  TOP
}

// Handles saving and syncing the scroll positions of the grid.
class ScrollHandler {
  divs: Divs;
  scrollTop: number;
  scrollLeft: number;
  scrollTopTimeout: NodeJS.Timeout;
  scrollLeftTimeout: NodeJS.Timeout;
  scrollTopFunc: (this: GlobalEventHandlers, ev: Event) => any;
  scrollLeftFunc: (this: GlobalEventHandlers, ev: Event) => any;

  constructor(divs: Divs) {
    this.divs = divs;
  }

  // This function is used to hide the rows which are not currently in view and
  // so reduce the performance cost of things like hit tests and scrolling.
  public syncHidden(): void {
    const toHide = new Array<[HTMLElement, HTMLElement]>();

    const sampleCell = this.divs.registers.children[1] as HTMLElement;
    const buffer = sampleCell.clientHeight * 2;
    const min = this.divs.grid.offsetTop + this.divs.grid.scrollTop - buffer;
    const max = min + this.divs.grid.clientHeight + buffer;

    // The rows are grouped by being contained within a group div. This is so as to allow
    // groups of rows to easily be displayed and hidden with less of a performance cost.
    // Each row in the mainGroup div is matched with an equivalent placeholder row in
    // the placeholderGroup div that will be shown when mainGroup is hidden so as to maintain
    // the dimensions and scroll positions of the grid.

    const rangeGroups = this.divs.grid.children;
    for (let i = 1; i < rangeGroups.length; i += 2) {
      const mainGroup = rangeGroups[i] as HTMLElement;
      const placeholderGroup = rangeGroups[i - 1] as HTMLElement;
      const isHidden = mainGroup.classList.contains("range-hidden");
      // The offsets are used to calculate whether the group is in view.
      const offsetMin = this.getOffset(mainGroup.firstChild as HTMLElement,
        placeholderGroup.firstChild as HTMLElement, isHidden);
      const offsetMax = this.getOffset(mainGroup.lastChild as HTMLElement,
        placeholderGroup.lastChild as HTMLElement, isHidden);

      if (offsetMax > min && offsetMin < max) {
        if (isHidden) {
          // Show the rows, hide the placeholders.
          mainGroup.classList.toggle("range-hidden", false);
          placeholderGroup.classList.toggle("range-hidden", true);
        }
      } else if (!isHidden) {
        // Only hide the rows once the new rows are shown so that scrollLeft is not lost.
        toHide.push([mainGroup, placeholderGroup]);
      }
    }

    for (const [mainGroup, placeholderGroup] of toHide) {
      // Hide the rows, show the placeholders.
      mainGroup.classList.toggle("range-hidden", true);
      placeholderGroup.classList.toggle("range-hidden", false);
    }
  }

  // This function is required to keep the axes labels in line with the grid
  // content when scrolling.
  public syncScroll(toSync: ToSync, source: HTMLElement, target: HTMLElement): void {
    // Continually delay timeout until scrolling has stopped.
    toSync == ToSync.TOP
      ? clearTimeout(this.scrollTopTimeout)
      : clearTimeout(this.scrollLeftTimeout);

    if (target.onscroll) {
      if (toSync == ToSync.TOP) {
        this.scrollTopFunc = target.onscroll;
      } else {
        this.scrollLeftFunc = target.onscroll;
      }
    }
    // Clear onscroll to prevent the target syncing back with the source.
    target.onscroll = null;

    if (toSync == ToSync.TOP) {
      target.scrollTop = source.scrollTop;
    } else {
      target.scrollLeft = source.scrollLeft;
    }

    // Only show / hide the grid content once scrolling has stopped.
    if (toSync == ToSync.TOP) {
      this.scrollTopTimeout = setTimeout(() => {
        target.onscroll = this.scrollTopFunc;
        this.syncHidden();
      }, 500);
    } else {
      this.scrollLeftTimeout = setTimeout(() => {
        target.onscroll = this.scrollLeftFunc;
        this.syncHidden();
      }, 500);
    }
  }

  public saveScroll(): void {
    this.scrollLeft = this.divs.grid.scrollLeft;
    this.scrollTop = this.divs.grid.scrollTop;
  }

  public restoreScroll(): void {
    if (this.scrollLeft) {
      this.divs.grid.scrollLeft = this.scrollLeft;
      this.divs.grid.scrollTop = this.scrollTop;
    }
  }

  private getOffset(rowEl: HTMLElement, placeholderRowEl: HTMLElement, isHidden: boolean): number {
    return isHidden ? placeholderRowEl.offsetTop : rowEl.offsetTop;
  }
}

// RangeView displays the live range data as passed in by SequenceView.
// The data is displayed in a grid format, with the fixed and virtual registers
// along one axis, and the LifeTimePositions along the other. Each LifeTimePosition
// is part of an Instruction in SequenceView, which itself is part of an Instruction
// Block. The live ranges are displayed as intervals, each belonging to a register,
// and spanning across a certain range of LifeTimePositions.
// When the phase being displayed changes between before register allocation and
// after register allocation, only the intervals need to be changed.
export class RangeView {
  sequenceView: SequenceView;
  gridAccessor: GridAccessor;
  intervalsAccessor: IntervalElementsAccessor;
  cssVariables: CSSVariables;
  userSettings: UserSettings;
  blocksData: BlocksData;
  divs: Divs;
  scrollHandler: ScrollHandler;
  phaseChangeHandler: PhaseChangeHandler;
  rowConstructor: RowConstructor;
  initialized: boolean;
  isShown: boolean;
  numPositions: number;

  constructor(sequence: SequenceView) {
    this.sequenceView = sequence;
    this.initialized = false;
    this.isShown = false;
  }

  public initializeContent(blocks: Array<SequenceBlock>): void {
    if (!this.initialized) {
      this.gridAccessor = new GridAccessor(this.sequenceView);
      this.intervalsAccessor = new IntervalElementsAccessor(this.sequenceView);
      this.cssVariables = new CSSVariables();
      this.userSettings = new UserSettings();
      // Indicates whether the RangeView is displayed beside or below the SequenceView.
      this.userSettings.addSetting("landscapeMode", false, this.resetLandscapeMode.bind(this));
      this.blocksData = new BlocksData(blocks);
      this.divs = new Divs(this.userSettings);
      this.scrollHandler = new ScrollHandler(this.divs);
      this.numPositions = this.sequenceView.numInstructions * C.POSITIONS_PER_INSTRUCTION;
      this.rowConstructor = new RowConstructor(this);
      const constructor = new RangeViewConstructor(this);
      constructor.construct();
      this.phaseChangeHandler = new PhaseChangeHandler(this);
      this.initialized = true;
    } else {
      // If the RangeView has already been initialized then the phase must have
      // been changed.
      this.phaseChangeHandler.phaseChange();
    }
  }

  public show(): void {
    if (!this.isShown) {
      this.isShown = true;
      this.divs.container.appendChild(this.divs.content);
      this.divs.resizerBar.style.visibility = "visible";
      this.divs.container.style.visibility = "visible";
      this.divs.snapper.style.visibility = "visible";
      // Dispatch a resize event to ensure that the
      // panel is shown.
      window.dispatchEvent(new Event("resize"));

      setTimeout(() => {
        this.userSettings.resetFromSessionStorage();
        this.scrollHandler.restoreScroll();
        this.scrollHandler.syncHidden();
        this.divs.showOnLoad.style.visibility = "visible";
      }, 100);
    }
  }

  public hide(): void {
    if (this.initialized) {
      this.isShown = false;
      this.divs.container.removeChild(this.divs.content);
      this.divs.resizerBar.style.visibility = "hidden";
      this.divs.container.style.visibility = "hidden";
      this.divs.snapper.style.visibility = "hidden";
      this.divs.showOnLoad.style.visibility = "hidden";
    } else {
      window.document.getElementById(C.RANGES_PANE_ID).style.visibility = "hidden";
    }
    // Dispatch a resize event to ensure that the
    // panel is hidden.
    window.dispatchEvent(new Event("resize"));
  }

  public onresize(): void {
    if (this.isShown) this.scrollHandler.syncHidden();
  }

  public resetLandscapeMode(isInLandscapeMode: boolean): void {
    // Used to communicate the setting to Resizer.
    this.divs.container.dataset.landscapeMode = isInLandscapeMode.toString();

    window.dispatchEvent(new Event("resize"));
    // Required to adjust scrollbar spacing.
    setTimeout(() => {
      window.dispatchEvent(new Event("resize"));
    }, 100);
  }
}
