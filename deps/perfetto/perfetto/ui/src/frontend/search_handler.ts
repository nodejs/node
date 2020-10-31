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

import {searchSegment} from '../base/binary_search';
import {Actions} from '../common/actions';
import {toNs} from '../common/time';

import {globals} from './globals';
import {scrollToTrackAndTs} from './scroll_helper';

export function executeSearch(reverse = false) {
  const state = globals.frontendLocalState;
  const index = state.searchIndex;
  const startNs = toNs(globals.frontendLocalState.visibleWindowTime.start);
  const endNs = toNs(globals.frontendLocalState.visibleWindowTime.end);
  const currentTs = globals.currentSearchResults.tsStarts[index];

  // If this is a new search or the currentTs is not in the viewport,
  // select the first/last item in the viewport.
  if (index === -1 || currentTs < startNs || currentTs > endNs) {
    if (reverse) {
      const [smaller,] =
        searchSegment(globals.currentSearchResults.tsStarts, endNs);
      globals.frontendLocalState.setSearchIndex(smaller);
    } else {
      const [, larger] =
          searchSegment(globals.currentSearchResults.tsStarts, startNs);
      globals.frontendLocalState.setSearchIndex(larger);
    }
  } else {
    // If the currentTs is in the viewport, increment the index.
    if (reverse) {
      globals.frontendLocalState.setSearchIndex(Math.max(index - 1, 0));
    } else {
      globals.frontendLocalState.setSearchIndex(Math.min(
          index + 1, globals.currentSearchResults.sliceIds.length - 1));
    }
  }
  selectCurrentSearchResult();

  // TODO(taylori): If the user does a search before any other selection,
  // the details panel will pop up when the search is executed. If the search
  // result is behind where the details panel appears then it won't get scrolled
  // to. This time delay is a workaround for this specific situation.
  // A better solution will be a callback that allows something to happen on the
  // first redraw after an Action is applied.
  const delay = index === -1 ? 50 : 0;
  setTimeout(() => moveViewportToCurrentSearch(), delay);
}

function moveViewportToCurrentSearch() {
  const searchIndex = globals.frontendLocalState.searchIndex;
  if (searchIndex === -1) return;
  const currentTs = globals.currentSearchResults.tsStarts[searchIndex];
  const trackId = globals.currentSearchResults.trackIds[searchIndex];
  scrollToTrackAndTs(trackId, currentTs);
}

function selectCurrentSearchResult() {
  const state = globals.frontendLocalState;
  const searchIndex = state.searchIndex;
  const source = globals.currentSearchResults.sources[searchIndex];
  const currentId = globals.currentSearchResults.sliceIds[searchIndex];
  const trackId = globals.currentSearchResults.trackIds[searchIndex];

  if (currentId === undefined) return;

  if (source === 'cpu') {
    globals.dispatch(Actions.selectSlice({id: currentId, trackId}));
  } else {
    globals.dispatch(Actions.selectChromeSlice({id: currentId, trackId}));
  }
}
