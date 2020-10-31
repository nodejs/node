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
import {getContainingTrackId} from '../common/state';
import {fromNs, TimeSpan, toNs} from '../common/time';

import {globals} from './globals';

/**
 * Given a timestamp, if |ts| is not currently in view move the view to
 * center |ts|, keeping the same zoom level.
 */
export function horizontalScrollToTs(ts: number) {
  const startNs = toNs(globals.frontendLocalState.visibleWindowTime.start);
  const endNs = toNs(globals.frontendLocalState.visibleWindowTime.end);
  const currentViewNs = endNs - startNs;
  if (ts < startNs || ts > endNs) {
    // TODO(taylori): This is an ugly jump, we should do a smooth pan instead.
    globals.frontendLocalState.updateVisibleTime(new TimeSpan(
        fromNs(ts - currentViewNs / 2), fromNs(ts + currentViewNs / 2)));
  }
}

/**
 * Given a start and end timestamp (in ns), move the view to center this range
 * and zoom to a level where the range is 1/5 of the viewport.
 */
export function horizontalScrollAndZoomToRange(startTs: number, endTs: number) {
  const visibleDur = globals.frontendLocalState.visibleWindowTime.end -
      globals.frontendLocalState.visibleWindowTime.start;
  const selectDur = endTs - startTs;
  const viewStartNs = toNs(globals.frontendLocalState.visibleWindowTime.start);
  const viewEndNs = toNs(globals.frontendLocalState.visibleWindowTime.end);
  if (selectDur / visibleDur < 0.05 || startTs < viewStartNs ||
      endTs > viewEndNs) {
    globals.frontendLocalState.updateVisibleTime(
        new TimeSpan(startTs - (selectDur * 2), endTs + (selectDur * 2)));
  }
}

/**
 * Given a track id, find a track with that id and scroll it into view. If the
 * track is nested inside a track group, scroll to that track group instead.
 * If |openGroup| then open the track group and scroll to the track.
 */
export function verticalScrollToTrack(
    trackId: string|number, openGroup = false) {
  const trackIdString = `${trackId}`;
  const track = document.querySelector('#track_' + trackIdString);

  if (track) {
    // block: 'nearest' means that it will only scroll if the track is not
    // currently in view.
    track.scrollIntoView({behavior: 'smooth', block: 'nearest'});
    return;
  }

  let trackGroup = null;
  const trackGroupId = getContainingTrackId(globals.state, trackIdString);
  if (trackGroupId) {
    trackGroup = document.querySelector('#track_' + trackGroupId);
  }

  if (!trackGroupId || !trackGroup) {
    console.error(`Can't scroll, track (${trackIdString}) not found.`);
    return;
  }

  // The requested track is inside a closed track group, either open the track
  // group and scroll to the track or just scroll to the track group.
  if (openGroup) {
    // After the track exists in the dom, it will be scrolled to.
    globals.frontendLocalState.scrollToTrackId = trackId;
    globals.dispatch(Actions.toggleTrackGroupCollapsed({trackGroupId}));
    return;
  } else {
    trackGroup.scrollIntoView({behavior: 'smooth', block: 'nearest'});
  }
}


/**
 * Scroll vertically and horizontally to reach track (|trackId|) at |ts|.
 */
export function scrollToTrackAndTs(
    trackId: string|number|undefined, ts: number, openGroup = false) {
  if (trackId !== undefined) {
    verticalScrollToTrack(trackId, openGroup);
  }
  horizontalScrollToTs(ts);
}