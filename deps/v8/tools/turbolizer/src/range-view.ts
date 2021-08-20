// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { createElement } from "../src/util";
import { SequenceView } from "../src/sequence-view";
import { RegisterAllocation, Range, ChildRange, Interval } from "../src/source-resolver";

class Constants {
  // Determines how many rows each div group holds for the purposes of
  // hiding by syncHidden.
  static readonly ROW_GROUP_SIZE = 20;
  static readonly POSITIONS_PER_INSTRUCTION = 4;
  static readonly FIXED_REGISTER_LABEL_WIDTH = 6;

  static readonly INTERVAL_TEXT_FOR_NONE = "none";
  static readonly INTERVAL_TEXT_FOR_CONST = "const";
  static readonly INTERVAL_TEXT_FOR_STACK = "stack:";
}

// This class holds references to the HTMLElements that represent each cell.
class Grid {
  elements: Array<Array<HTMLElement>>;

  constructor() {
    this.elements = [];
  }

  setRow(row: number, elementsRow: Array<HTMLElement>) {
    this.elements[row] = elementsRow;
  }

  getCell(row: number, column: number) {
    return this.elements[row][column];
  }

  getInterval(row: number, column: number) {
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

  private currentGrid() {
    return this.grids.get(this.sequenceView.currentPhaseIndex);
  }

  getAnyGrid() {
    return this.grids.values().next().value;
  }

  hasGrid() {
    return this.grids.has(this.sequenceView.currentPhaseIndex);
  }

  addGrid(grid: Grid) {
    if (this.hasGrid()) console.warn("Overwriting existing Grid.");
    this.grids.set(this.sequenceView.currentPhaseIndex, grid);
  }

  getCell(row: number, column: number) {
    return this.currentGrid().getCell(row, column);
  }

  getInterval(row: number, column: number) {
    return this.currentGrid().getInterval(row, column);
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

  private currentIntervals() {
    const intervals = this.map.get(this.sequenceView.currentPhaseIndex);
    if (intervals == undefined) {
      this.map.set(this.sequenceView.currentPhaseIndex, new Array<HTMLElement>());
      return this.currentIntervals();
    }
    return intervals;
  }

  addInterval(interval: HTMLElement) {
    this.currentIntervals().push(interval);
  }

  forEachInterval(callback: (phase: number, interval: HTMLElement) => void) {
    for (const phase of this.map.keys()) {
      for (const interval of this.map.get(phase)) {
        callback(phase, interval);
      }
    }
  }
}

// A simple class used to hold two Range objects. This is used to allow the two fixed register live
// ranges of normal and deferred to be easily combined into a single row.
class RangePair {
  ranges: [Range, Range];

  constructor(ranges: [Range, Range]) {
    this.ranges = ranges;
  }

  forEachRange(callback: (range: Range) => void) {
    this.ranges.forEach((range: Range) => { if (range) callback(range); });
  }
}

// A number of css variables regarding dimensions of HTMLElements are required by RangeView.
class CSSVariables {
  positionWidth: number;
  blockBorderWidth: number;

  constructor() {
    const getNumberValue = varName => {
      return parseFloat(getComputedStyle(document.body)
             .getPropertyValue(varName).match(/[+-]?\d+(\.\d+)?/g)[0]);
    };
    this.positionWidth = getNumberValue("--range-position-width");
    this.blockBorderWidth = getNumberValue("--range-block-border");
  }
}

// Store the required data from the blocks JSON.
class BlocksData {
  blockBorders: Set<number>;
  blockInstructionCountMap: Map<number, number>;

  constructor(blocks: Array<any>) {
    this.blockBorders = new Set<number>();
    this.blockInstructionCountMap = new Map<number, number>();
    for (const block of blocks) {
      this.blockInstructionCountMap.set(block.id, block.instructions.length);
      const maxInstructionInBlock = block.instructions[block.instructions.length - 1].id;
      this.blockBorders.add(maxInstructionInBlock);
    }
  }

  isInstructionBorder(position: number) {
    return ((position + 1) % Constants.POSITIONS_PER_INSTRUCTION) == 0;
  }

  isBlockBorder(position: number) {
    return this.isInstructionBorder(position)
        && this.blockBorders.has(Math.floor(position / Constants.POSITIONS_PER_INSTRUCTION));
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

  constructor() {
    this.container = document.getElementById("ranges");
    this.resizerBar = document.getElementById("resizer-ranges");
    this.snapper = document.getElementById("show-hide-ranges");

    this.content = document.createElement("div");
    this.content.appendChild(this.elementForTitle());

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

  elementForTitle() {
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
    return titleEl;
  }
}

class Helper {
  static virtualRegisterName(registerIndex: string) {
    return "v" + registerIndex;
  }

  static fixedRegisterName(range: Range) {
    return range.child_ranges[0].op.text;
  }

  static getPositionElementsFromInterval(interval: HTMLElement) {
    return interval.children[1].children;
  }

  static forEachFixedRange(source: RegisterAllocation, row: number,
                           callback: (registerIndex: string, row: number, registerName: string,
                                      ranges: RangePair) => void) {

    const forEachRangeInMap = (rangeMap: Map<string, Range>) => {
      // There are two fixed live ranges for each register, one for normal, another for deferred.
      // These are combined into a single row.
      const fixedRegisterMap = new Map<string, {ranges: [Range, Range], registerIndex: number}>();
      for (const [registerIndex, range] of rangeMap) {
        const registerName = this.fixedRegisterName(range);
        if (fixedRegisterMap.has(registerName)) {
          const entry = fixedRegisterMap.get(registerName);
          entry.ranges[1] = range;
          // Only use the deferred register index if no normal index exists.
          if (!range.is_deferred) {
            entry.registerIndex = parseInt(registerIndex, 10);
          }
        } else {
          fixedRegisterMap.set(registerName, {ranges: [range, undefined],
                                              registerIndex: parseInt(registerIndex, 10)});
        }
      }
      // Sort the registers by number.
      const sortedMap = new Map([...fixedRegisterMap.entries()].sort(([nameA, _], [nameB, __]) => {
        // Larger numbers create longer strings.
        if (nameA.length > nameB.length) return 1;
        if (nameA.length < nameB.length) return -1;
        // Sort lexicographically if same length.
        if (nameA > nameB) return 1;
        if (nameA < nameB) return -1;
        return 0;
      }));
      for (const [registerName, {ranges, registerIndex}] of sortedMap) {
        callback("" + (-registerIndex - 1), row, registerName, new RangePair(ranges));
        ++row;
      }
    };

    forEachRangeInMap(source.fixedLiveRanges);
    forEachRangeInMap(source.fixedDoubleLiveRanges);

    return row;
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
  construct(grid: Grid, row: number, registerIndex: string, ranges: RangePair,
            getElementForEmptyPosition: (position: number) => HTMLElement,
            callbackForInterval: (position: number, interval: HTMLElement) => void) {
    const positionArray = new Array<HTMLElement>(this.view.numPositions);
    // Construct all of the new intervals.
    const intervalMap = this.elementsForIntervals(registerIndex, ranges);
    for (let position = 0; position < this.view.numPositions; ++position) {
      const interval = intervalMap.get(position);
      if (interval == undefined) {
        positionArray[position] = getElementForEmptyPosition(position);
      } else {
        callbackForInterval(position, interval);
        this.view.intervalsAccessor.addInterval(interval);
        const intervalPositionElements = Helper.getPositionElementsFromInterval(interval);
        for (let j = 0; j < intervalPositionElements.length; ++j) {
          // Point positionsArray to the new elements.
          positionArray[position + j] = (intervalPositionElements[j] as HTMLElement);
        }
        position += intervalPositionElements.length - 1;
      }
    }
    grid.setRow(row, positionArray);
    ranges.forEachRange((range: Range) => this.setUses(grid, row, range));
  }

  // This is the main function used to build new intervals.
  // Returns a map of LifeTimePositions to intervals.
  private elementsForIntervals(registerIndex: string, ranges: RangePair) {
    const intervalMap = new Map<number, HTMLElement>();
    let tooltip = "";
    ranges.forEachRange((range: Range) => {
      for (const childRange of range.child_ranges) {
        switch (childRange.type) {
          case "none":
            tooltip = Constants.INTERVAL_TEXT_FOR_NONE;
            break;
          case "spill_range":
            tooltip = Constants.INTERVAL_TEXT_FOR_STACK + registerIndex;
            break;
          default:
            if (childRange.op.type == "constant") {
              tooltip = Constants.INTERVAL_TEXT_FOR_CONST;
            } else {
              if (childRange.op.text) {
                tooltip = childRange.op.text;
              } else {
                tooltip = childRange.op;
              }
            }
            break;
        }
        childRange.intervals.forEach((intervalNums, index) => {
          const interval = new Interval(intervalNums);
          const intervalEl = this.elementForInterval(childRange, interval, tooltip,
                                                     index, range.is_deferred);
          intervalMap.set(interval.start, intervalEl);
        });
      }
    });
    return intervalMap;
  }

  private elementForInterval(childRange: ChildRange, interval: Interval,
                             tooltip: string, index: number, isDeferred: boolean): HTMLElement {
    const intervalEl = createElement("div", "range-interval");
    const title = childRange.id + ":" + index + " " + tooltip;
    intervalEl.setAttribute("title", isDeferred ? "deferred: " + title : title);
    this.setIntervalColor(intervalEl, tooltip);
    const intervalInnerWrapper = createElement("div", "range-interval-wrapper");
    intervalEl.style.gridColumn = (interval.start + 1) + " / " + (interval.end + 1);
    intervalInnerWrapper.style.gridTemplateColumns = "repeat(" + (interval.end - interval.start)
                                        + ",calc(" + this.view.cssVariables.positionWidth + "ch + "
                                        + this.view.cssVariables.blockBorderWidth + "px)";
    const intervalTextEl = this.elementForIntervalString(tooltip, interval.end - interval.start);
    intervalEl.appendChild(intervalTextEl);
    for (let i = interval.start; i < interval.end; ++i) {
      const classes = "range-position range-interval-position range-empty"
                    + (this.view.blocksData.isBlockBorder(i) ? " range-block-border" :
                      this.view.blocksData.isInstructionBorder(i) ? " range-instr-border" : "");
      const positionEl = createElement("div", classes, "_");
      positionEl.style.gridColumn = (i - interval.start + 1) + "";
      intervalInnerWrapper.appendChild(positionEl);
    }
    intervalEl.appendChild(intervalInnerWrapper);
    return intervalEl;
  }

  private setIntervalColor(interval: HTMLElement, tooltip: string) {
    if (tooltip.includes(Constants.INTERVAL_TEXT_FOR_NONE)) return;
    if (tooltip.includes(Constants.INTERVAL_TEXT_FOR_STACK + "-")) {
      interval.style.backgroundColor = "rgb(250, 158, 168)";
    } else if (tooltip.includes(Constants.INTERVAL_TEXT_FOR_STACK)) {
      interval.style.backgroundColor = "rgb(250, 158, 100)";
    } else if (tooltip.includes(Constants.INTERVAL_TEXT_FOR_CONST)) {
      interval.style.backgroundColor = "rgb(153, 158, 230)";
    } else {
      interval.style.backgroundColor = "rgb(153, 220, 168)";
    }
  }

  private elementForIntervalString(tooltip: string, numCells: number) {
    const spanEl = createElement("span", "range-interval-text");
    this.setIntervalString(spanEl, tooltip, numCells);
    return spanEl;
  }

  // Each interval displays a string of information about it.
  private setIntervalString(spanEl: HTMLElement, tooltip: string, numCells: number) {
    const spacePerCell = this.view.cssVariables.positionWidth;
    // One character space is removed to accommodate for padding.
    const spaceAvailable = (numCells * spacePerCell) - 0.5;
    let str = tooltip + "";
    const length = tooltip.length;
    spanEl.style.width = null;
    let paddingLeft = null;
    // Add padding if possible
    if (length <= spaceAvailable) {
      paddingLeft = (length == spaceAvailable) ? "0.5ch" : "1ch";
    } else {
      str = "";
    }
    spanEl.style.paddingTop = null;
    spanEl.style.paddingLeft = paddingLeft;
    spanEl.innerHTML = str;
  }

  private setUses(grid: Grid, row: number, range: Range) {
    for (const liveRange of range.child_ranges) {
      if (liveRange.uses) {
        for (const use of liveRange.uses) {
          grid.getCell(row, use).classList.toggle("range-use", true);
        }
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

  construct() {
    this.gridTemplateColumns = "repeat(" + this.view.numPositions
                             + ",calc(" + this.view.cssVariables.positionWidth + "ch + "
                             + this.view.cssVariables.blockBorderWidth + "px)";

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
    let row = 0;
    row = this.addVirtualRanges(row);
    this.addFixedRanges(row);
  }

  // The following three functions are for constructing the groups which the rows are contained
  // within and which make up the grid. This is so as to allow groups of rows to easily be displayed
  // and hidden for performance reasons. As rows are constructed, they are added to the currentGroup
  // div. Each row in currentGroup is matched with an equivalent placeholder row in
  // currentPlaceholderGroup that will be shown when currentGroup is hidden so as to maintain the
  // dimensions and scroll positions of the grid.

  private resetGroups () {
    this.currentGroup = createElement("div", "range-positions-group range-hidden");
    this.currentPlaceholderGroup = createElement("div", "range-positions-group");
  }

  private appendGroupsToGrid() {
    this.view.divs.grid.appendChild(this.currentPlaceholderGroup);
    this.view.divs.grid.appendChild(this.currentGroup);
  }

  private addRowToGroup(row: number, rowEl: HTMLElement) {
    this.currentGroup.appendChild(rowEl);
    this.currentPlaceholderGroup
        .appendChild(createElement("div", "range-positions range-positions-placeholder", "_"));
    if ((row + 1) % Constants.ROW_GROUP_SIZE == 0) {
      this.appendGroupsToGrid();
      this.resetGroups();
    }
  }

  private addVirtualRanges(row: number) {
    const source = this.view.sequenceView.sequence.register_allocation;
    for (const [registerIndex, range] of source.liveRanges) {
      const registerName = Helper.virtualRegisterName(registerIndex);
      const registerEl = this.elementForVirtualRegister(registerName);
      this.addRowToGroup(row, this.elementForRow(row, registerIndex,
                                                 new RangePair([range, undefined])));
      this.view.divs.registers.appendChild(registerEl);
      ++row;
    }
    return row;
  }

  private addFixedRanges(row: number) {
    row = Helper.forEachFixedRange(this.view.sequenceView.sequence.register_allocation, row,
                                   (registerIndex: string, row: number,
                                    registerName: string, ranges: RangePair) => {
      const registerEl = this.elementForFixedRegister(registerName);
      this.addRowToGroup(row, this.elementForRow(row, registerIndex, ranges));
      this.view.divs.registers.appendChild(registerEl);
    });
    if (row % Constants.ROW_GROUP_SIZE != 0) {
      this.appendGroupsToGrid();
    }
  }

  // Each row of positions and intervals associated with a register is contained in a single
  // HTMLElement. RangePair is used to allow the two fixed register live ranges of normal and
  // deferred to be easily combined into a single row.
  private elementForRow(row: number, registerIndex: string, ranges: RangePair) {
    const rowEl = createElement("div", "range-positions");
    rowEl.style.gridTemplateColumns = this.gridTemplateColumns;

    const getElementForEmptyPosition = (position: number) => {
      const blockBorder = this.view.blocksData.isBlockBorder(position);
      const classes = "range-position range-empty "
                    + (blockBorder ? "range-block-border" :
                    this.view.blocksData.isInstructionBorder(position) ? "range-instr-border"
                                                                       : "range-position-border");
      const positionEl = createElement("div", classes, "_");
      positionEl.style.gridColumn = (position + 1) + "";
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

  private elementForVirtualRegister(registerName: string) {
    const regEl = createElement("div", "range-reg", registerName);
    regEl.setAttribute("title", registerName);
    return regEl;
  }

  private elementForFixedRegister(registerName: string) {
    let text = registerName;
    const span = "".padEnd(Constants.FIXED_REGISTER_LABEL_WIDTH - text.length, "_");
    text = "HW - <span class='range-transparent'>" + span + "</span>" + text;
    const regEl = createElement("div", "range-reg");
    regEl.innerHTML = text;
    regEl.setAttribute("title", registerName);
    return regEl;
  }

  // The header element contains the three headers for the LifeTimePosition axis.
  private elementForHeader() {
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

  private elementForBlockHeader() {
    const headerEl = createElement("div", "range-block-ids");
    headerEl.style.gridTemplateColumns = this.gridTemplateColumns;

    const elementForBlockIndex = (index: number, firstInstruction: number, instrCount: number) => {
      const str = "B" + index;
      const element =
        createElement("div", "range-block-id range-header-element range-block-border", str);
      element.setAttribute("title", str);
      const firstGridCol = (firstInstruction * Constants.POSITIONS_PER_INSTRUCTION) + 1;
      const lastGridCol = firstGridCol + (instrCount * Constants.POSITIONS_PER_INSTRUCTION);
      element.style.gridColumn = firstGridCol + " / " + lastGridCol;
      return element;
    };

    let blockIndex = 0;
    for (let i = 0; i < this.view.sequenceView.numInstructions;) {
      const instrCount = this.view.blocksData.blockInstructionCountMap.get(blockIndex);
      headerEl.appendChild(elementForBlockIndex(blockIndex, i, instrCount));
      ++blockIndex;
      i += instrCount;
    }
    return headerEl;
  }

  private elementForInstructionHeader() {
    const headerEl = createElement("div", "range-instruction-ids");
    headerEl.style.gridTemplateColumns = this.gridTemplateColumns;

    const elementForInstructionIndex = (index: number, isBlockBorder: boolean) => {
      const classes = "range-instruction-id range-header-element "
                    + (isBlockBorder ? "range-block-border" : "range-instr-border");
      const element = createElement("div", classes, "" + index);
      element.setAttribute("title", "" + index);
      const firstGridCol = (index * Constants.POSITIONS_PER_INSTRUCTION) + 1;
      element.style.gridColumn = firstGridCol + " / "
                               + (firstGridCol + Constants.POSITIONS_PER_INSTRUCTION);
      return element;
    };

    for (let i = 0; i < this.view.sequenceView.numInstructions; ++i) {
      const blockBorder = this.view.blocksData.blockBorders.has(i);
      headerEl.appendChild(elementForInstructionIndex(i, blockBorder));
    }
    return headerEl;
  }

  private elementForPositionHeader() {
    const headerEl = createElement("div", "range-positions range-positions-header");
    headerEl.style.gridTemplateColumns = this.gridTemplateColumns;

    const elementForPositionIndex = (index: number, isBlockBorder: boolean) => {
      const classes = "range-position range-header-element " +
        (isBlockBorder ? "range-block-border"
                       : this.view.blocksData.isInstructionBorder(index) ? "range-instr-border"
                                                                         : "range-position-border");
      const element = createElement("div", classes, "" + index);
      element.setAttribute("title", "" + index);
      return element;
    };

    for (let i = 0; i < this.view.numPositions; ++i) {
      headerEl.appendChild(elementForPositionIndex(i, this.view.blocksData.isBlockBorder(i)));
    }
    return headerEl;
  }

  private elementForGrid() {
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
  phaseChange() {
    if (!this.view.gridAccessor.hasGrid()) {
      // If this phase view has not been seen yet then the intervals need to be constructed.
      this.addNewIntervals();
    }
    // Show all intervals pertaining to the current phase view.
    this.view.intervalsAccessor.forEachInterval((phase, interval) => {
      interval.classList.toggle("range-hidden", phase != this.view.sequenceView.currentPhaseIndex);
    });
  }

  private addNewIntervals() {
    // All Grids should point to the same HTMLElement for empty cells in the grid,
    // so as to avoid duplication. The current Grid is used to retrieve these elements.
    const currentGrid = this.view.gridAccessor.getAnyGrid();
    const newGrid = new Grid();
    this.view.gridAccessor.addGrid(newGrid);
    const source = this.view.sequenceView.sequence.register_allocation;
    let row = 0;
    for (const [registerIndex, range] of source.liveRanges) {
      this.addnewIntervalsInRange(currentGrid, newGrid, row, registerIndex,
                                  new RangePair([range, undefined]));
      ++row;
    }
    Helper.forEachFixedRange(this.view.sequenceView.sequence.register_allocation, row,
                             (registerIndex, row, _, ranges) => {
      this.addnewIntervalsInRange(currentGrid, newGrid, row, registerIndex, ranges);
    });
  }

  private addnewIntervalsInRange(currentGrid: Grid, newGrid: Grid, row: number,
                                 registerIndex: string, ranges: RangePair) {
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
      numReplacements.set(currentInterval, intervalsAlreadyInserted ? intervalsAlreadyInserted + 1
                                                                    : 1);
      if (intervalsAlreadyInserted) {
        for (let j = 0; j < intervalsAlreadyInserted; ++j) {
          currentInterval = (currentInterval.nextElementSibling as HTMLElement);
        }
      }
      interval.classList.add("range-hidden");
      currentInterval.insertAdjacentElement('afterend', interval);
    };

    this.view.rowConstructor.construct(newGrid, row, registerIndex, ranges,
                                          getElementForEmptyPosition, callbackForInterval);
  }
}

enum ToSync { LEFT, TOP }

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
  syncHidden() {

    const getOffset = (rowEl: HTMLElement, placeholderRowEl: HTMLElement, isHidden: boolean) => {
      return isHidden ? placeholderRowEl.offsetTop : rowEl.offsetTop;
    };

    const toHide = new Array<[HTMLElement, HTMLElement]>();

    const sampleCell = this.divs.registers.children[1] as HTMLElement;
    const buffer = 2 * sampleCell.clientHeight;
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
      const offsetMin = getOffset(mainGroup.firstChild as HTMLElement,
                                  placeholderGroup.firstChild as HTMLElement, isHidden);
      const offsetMax = getOffset(mainGroup.lastChild as HTMLElement,
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
  syncScroll(toSync: ToSync, source: HTMLElement, target: HTMLElement) {
    // Continually delay timeout until scrolling has stopped.
    toSync == ToSync.TOP ? clearTimeout(this.scrollTopTimeout)
                         : clearTimeout(this.scrollLeftTimeout);
    if (target.onscroll) {
      if (toSync == ToSync.TOP) this.scrollTopFunc = target.onscroll;
      else this.scrollLeftFunc = target.onscroll;
    }
    // Clear onscroll to prevent the target syncing back with the source.
    target.onscroll = null;

    if (toSync == ToSync.TOP) target.scrollTop = source.scrollTop;
    else target.scrollLeft = source.scrollLeft;

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

  saveScroll() {
    this.scrollLeft = this.divs.grid.scrollLeft;
    this.scrollTop = this.divs.grid.scrollTop;
  }

  restoreScroll() {
    if (this.scrollLeft) {
      this.divs.grid.scrollLeft = this.scrollLeft;
      this.divs.grid.scrollTop = this.scrollTop;
    }
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

  initialized: boolean;
  isShown: boolean;
  numPositions: number;
  cssVariables: CSSVariables;
  divs: Divs;
  rowConstructor: RowConstructor;
  phaseChangeHandler: PhaseChangeHandler;
  scrollHandler: ScrollHandler;
  blocksData: BlocksData;
  intervalsAccessor: IntervalElementsAccessor;
  gridAccessor: GridAccessor;

  constructor(sequence: SequenceView) {
    this.initialized = false;
    this.isShown = false;
    this.sequenceView = sequence;
  }

  initializeContent(blocks: Array<any>) {
    if (!this.initialized) {
      this.gridAccessor = new GridAccessor(this.sequenceView);
      this.intervalsAccessor = new IntervalElementsAccessor(this.sequenceView);
      this.cssVariables = new CSSVariables();
      this.blocksData = new BlocksData(blocks);
      this.divs = new Divs();
      this.scrollHandler = new ScrollHandler(this.divs);
      this.numPositions = this.sequenceView.numInstructions * Constants.POSITIONS_PER_INSTRUCTION;
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

  show() {
    if (!this.isShown) {
      this.isShown = true;
      this.divs.container.appendChild(this.divs.content);
      this.divs.resizerBar.style.visibility = "visible";
      this.divs.container.style.visibility = "visible";
      this.divs.snapper.style.visibility = "visible";
      // Dispatch a resize event to ensure that the
      // panel is shown.
      window.dispatchEvent(new Event('resize'));

      setTimeout(() => {
        this.scrollHandler.restoreScroll();
        this.scrollHandler.syncHidden();
        this.divs.showOnLoad.style.visibility = "visible";
      }, 100);
    }
  }

  hide() {
    if (this.initialized) {
      this.isShown = false;
      this.divs.container.removeChild(this.divs.content);
      this.divs.resizerBar.style.visibility = "hidden";
      this.divs.container.style.visibility = "hidden";
      this.divs.snapper.style.visibility = "hidden";
      this.divs.showOnLoad.style.visibility = "hidden";
    } else {
      window.document.getElementById('ranges').style.visibility = "hidden";
    }
    // Dispatch a resize event to ensure that the
    // panel is hidden.
    window.dispatchEvent(new Event('resize'));
  }

  onresize() {
    if (this.isShown) this.scrollHandler.syncHidden();
  }
}
