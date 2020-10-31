// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {Actions} from '../common/actions';

import {globals} from './globals';
import {toggleHelp} from './help_modal';
import {
  horizontalScrollAndZoomToRange,
  verticalScrollToTrack
} from './scroll_helper';
import {executeSearch} from './search_handler';

// Handles all key events than are not handled by the
// pan and zoom handler.
export function handleKey(e: KeyboardEvent, down: boolean) {
  const key = e.key.toLowerCase();
  if (down && 'm' === key) {
    const selectedArea = globals.frontendLocalState.selectedArea.area;
    if (!selectedArea && globals.state.currentSelection !== null) {
      selectSliceSpan();
    }
    if (selectedArea) {
      globals.frontendLocalState.toggleLockArea();
    }
  }
  if (down && 'f' === key) {
    findCurrentSelection();
  }
  if (down && 'v' === key) {
    globals.dispatch(Actions.toggleVideo({}));
  }
  if (down && 'p' === key) {
    globals.dispatch(Actions.toggleFlagPause({}));
  }
  if (down && 't' === key) {
    globals.dispatch(Actions.toggleScrubbing({}));
    if (globals.frontendLocalState.vidTimestamp < 0) {
      globals.frontendLocalState.setVidTimestamp(Number.MAX_SAFE_INTEGER);
    } else {
      globals.frontendLocalState.setVidTimestamp(Number.MIN_SAFE_INTEGER);
    }
  }
  if (down && 'b' === key && (e.ctrlKey || e.metaKey)) {
    globals.frontendLocalState.toggleSidebar();
  }
  if (down && '?' === key) {
    toggleHelp();
  }
  if (down && 'enter' === key) {
    e.preventDefault();
    executeSearch(e.shiftKey);
  }
}

function findTimeRangeOfSelection() {
  const selection = globals.state.currentSelection;
  let startTs = -1;
  let endTs = -1;
  if (selection !== null) {
    if (selection.kind === 'SLICE' || selection.kind === 'CHROME_SLICE') {
      const slice = globals.sliceDetails;
      if (slice.ts && slice.dur) {
        startTs = slice.ts + globals.state.traceTime.startSec;
        endTs = startTs + slice.dur;
      }
    } else if (selection.kind === 'THREAD_STATE') {
      startTs = selection.ts;
      endTs = startTs + selection.dur;
    } else if (selection.kind === 'COUNTER') {
      startTs = selection.leftTs;
      endTs = selection.rightTs;
    }
  }
  return {startTs, endTs};
}

function selectSliceSpan() {
  const range = findTimeRangeOfSelection();
  if (range.startTs !== -1 && range.endTs !== -1 &&
      globals.state.currentSelection) {
    const tracks = globals.state.currentSelection.trackId ?
        [globals.state.currentSelection.trackId] :
        [];
    globals.frontendLocalState.selectArea(range.startTs, range.endTs, tracks);
  }
}

function findCurrentSelection() {
  const selection = globals.state.currentSelection;
  if (selection === null) return;

  const range = findTimeRangeOfSelection();
  if (range.startTs !== -1 && range.endTs !== -1) {
    horizontalScrollAndZoomToRange(range.startTs, range.endTs);
  }

  if (selection.trackId) {
    verticalScrollToTrack(selection.trackId, true);
  }
}
