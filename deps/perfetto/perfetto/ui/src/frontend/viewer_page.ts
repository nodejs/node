// Copyright (C) 2018 The Android Open Source Project
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

import * as m from 'mithril';
import {Row} from 'src/common/protos';

import {Actions} from '../common/actions';
import {QueryResponse} from '../common/queries';
import {fromNs, TimeSpan} from '../common/time';

import {copyToClipboard} from './clipboard';
import {TRACK_SHELL_WIDTH} from './css_constants';
import {DetailsPanel} from './details_panel';
import {globals} from './globals';
import {NotesPanel} from './notes_panel';
import {OverviewTimelinePanel} from './overview_timeline_panel';
import {createPage} from './pages';
import {PanAndZoomHandler} from './pan_and_zoom_handler';
import {Panel} from './panel';
import {AnyAttrsVnode, PanelContainer} from './panel_container';
import {
  horizontalScrollAndZoomToRange,
  verticalScrollToTrack
} from './scroll_helper';
import {TickmarkPanel} from './tickmark_panel';
import {TimeAxisPanel} from './time_axis_panel';
import {computeZoom} from './time_scale';
import {TimeSelectionPanel} from './time_selection_panel';
import {DISMISSED_PANNING_HINT_KEY} from './topbar';
import {TrackGroupPanel} from './track_group_panel';
import {TrackPanel} from './track_panel';
import {VideoPanel} from './video_panel';

const SIDEBAR_WIDTH = 256;

interface QueryTableRowAttrs {
  row: Row;
  columns: string[];
}

class QueryTableRow implements m.ClassComponent<QueryTableRowAttrs> {
  static columnsContainsSliceLocation(columns: string[]) {
    const requiredColumns = ['ts', 'dur', 'track_id'];
    for (const col of requiredColumns) {
      if (!columns.includes(col)) return false;
    }
    return true;
  }

  static findUiTrackId(traceTrackId: number) {
    for (const [uiTrackId, trackState] of Object.entries(
             globals.state.tracks)) {
      const config = trackState.config as {trackId: number};
      if (config.trackId === traceTrackId) return uiTrackId;
    }
    return null;
  }

  static rowOnClickHandler(event: Event, row: Row) {
    // If the click bubbles up to the pan and zoom handler that will deselect
    // the slice.
    event.stopPropagation();

    const sliceStart = fromNs(row.ts as number);
    // row.dur can be negative. Clamp to 1ns.
    const sliceDur = fromNs(Math.max(row.dur as number, 1));
    const sliceEnd = sliceStart + sliceDur;
    const trackId = row.track_id as number;
    const uiTrackId = this.findUiTrackId(trackId);
    if (uiTrackId === null) return;
    verticalScrollToTrack(uiTrackId, true);
    horizontalScrollAndZoomToRange(sliceStart, sliceEnd);
    const sliceId = row.slice_id as number | undefined;
    if (sliceId !== undefined) {
      globals.makeSelection(
          Actions.selectChromeSlice({id: sliceId, trackId: uiTrackId}));
    }
  }

  view(vnode: m.Vnode<QueryTableRowAttrs>) {
    const cells = [];
    const {row, columns} = vnode.attrs;
    for (const col of columns) {
      cells.push(m('td', row[col]));
    }
    const containsSliceLocation =
        QueryTableRow.columnsContainsSliceLocation(columns);
    const maybeOnClick = containsSliceLocation ?
        (e: Event) => QueryTableRow.rowOnClickHandler(e, row) :
        null;
    return m(
        'tr',
        {onclick: maybeOnClick, 'clickable': containsSliceLocation},
        cells);
  }
}

class QueryTable extends Panel {
  private previousResponse?: QueryResponse;

  onbeforeupdate() {
    const resp = globals.queryResults.get('command') as QueryResponse;
    const res = resp !== this.previousResponse;
    return res;
  }

  view() {
    const resp = globals.queryResults.get('command') as QueryResponse;
    if (resp === undefined) {
      return m('');
    }
    this.previousResponse = resp;
    const cols = [];
    for (const col of resp.columns) {
      cols.push(m('td', col));
    }
    const header = m('tr', cols);

    const rows = [];
    for (let i = 0; i < resp.rows.length; i++) {
      rows.push(m(QueryTableRow, {row: resp.rows[i], columns: resp.columns}));
    }

    return m(
        'div',
        m(
            'header.overview',
            `Query result - ${Math.round(resp.durationMs)} ms`,
            m('span.code', resp.query),
            resp.error ?
                null :
                m('button.query-ctrl',
                  {
                    onclick: () => {
                      const lines: string[][] = [];
                      lines.push(resp.columns);
                      for (const row of resp.rows) {
                        const line = [];
                        for (const col of resp.columns) {
                          line.push(row[col].toString());
                        }
                        lines.push(line);
                      }
                      copyToClipboard(
                          lines.map(line => line.join('\t')).join('\n'));
                    },
                  },
                  'Copy as .tsv'),
            m('button.query-ctrl',
              {
                onclick: () => {
                  globals.queryResults.delete('command');
                  globals.rafScheduler.scheduleFullRedraw();
                }
              },
              'Close'),
            ),
        resp.error ?
            m('.query-error', `SQL error: ${resp.error}`) :
            m('table.query-table', m('thead', header), m('tbody', rows)));
  }

  renderCanvas() {}
}


// Checks if the mousePos is within 3px of the start or end of the
// current selected time range.
function onTimeRangeBoundary(mousePos: number): 'START'|'END'|null {
  const area = globals.frontendLocalState.selectedArea.area;
  if (area !== undefined) {
    const start = globals.frontendLocalState.timeScale.timeToPx(area.startSec);
    const end = globals.frontendLocalState.timeScale.timeToPx(area.endSec);
    const startDrag = mousePos - TRACK_SHELL_WIDTH;
    const startDistance = Math.abs(start - startDrag);
    const endDistance = Math.abs(end - startDrag);
    const range = 3 * window.devicePixelRatio;
    // We might be within 3px of both boundaries but we should choose
    // the closest one.
    if (startDistance < range && startDistance <= endDistance) return 'START';
    if (endDistance < range && endDistance <= startDistance) return 'END';
  }
  return null;
}

/**
 * Top-most level component for the viewer page. Holds tracks, brush timeline,
 * panels, and everything else that's part of the main trace viewer page.
 */
class TraceViewer implements m.ClassComponent {
  private onResize: () => void = () => {};
  private zoomContent?: PanAndZoomHandler;
  // Used to prevent global deselection if a pan/drag select occurred.
  private keepCurrentSelection = false;

  oncreate(vnode: m.CVnodeDOM) {
    const frontendLocalState = globals.frontendLocalState;
    const updateDimensions = () => {
      const rect = vnode.dom.getBoundingClientRect();
      frontendLocalState.updateResolution(
          0,
          rect.width - TRACK_SHELL_WIDTH -
              frontendLocalState.getScrollbarWidth());
    };

    updateDimensions();

    // TODO: Do resize handling better.
    this.onResize = () => {
      updateDimensions();
      globals.rafScheduler.scheduleFullRedraw();
    };

    // Once ResizeObservers are out, we can stop accessing the window here.
    window.addEventListener('resize', this.onResize);

    const panZoomEl =
        vnode.dom.querySelector('.pan-and-zoom-content') as HTMLElement;

    this.zoomContent = new PanAndZoomHandler({
      element: panZoomEl,
      contentOffsetX: SIDEBAR_WIDTH,
      onPanned: (pannedPx: number) => {
        this.keepCurrentSelection = true;
        const traceTime = globals.state.traceTime;
        const vizTime = globals.frontendLocalState.visibleWindowTime;
        const origDelta = vizTime.duration;
        const tDelta = frontendLocalState.timeScale.deltaPxToDuration(pannedPx);
        let tStart = vizTime.start + tDelta;
        let tEnd = vizTime.end + tDelta;
        if (tStart < traceTime.startSec) {
          tStart = traceTime.startSec;
          tEnd = tStart + origDelta;
        } else if (tEnd > traceTime.endSec) {
          tEnd = traceTime.endSec;
          tStart = tEnd - origDelta;
        }
        frontendLocalState.updateVisibleTime(new TimeSpan(tStart, tEnd));
        // If the user has panned they no longer need the hint.
        localStorage.setItem(DISMISSED_PANNING_HINT_KEY, 'true');
        globals.rafScheduler.scheduleRedraw();
      },
      onZoomed: (zoomedPositionPx: number, zoomRatio: number) => {
        // TODO(hjd): Avoid hardcoding TRACK_SHELL_WIDTH.
        // TODO(hjd): Improve support for zooming in overview timeline.
        const span = frontendLocalState.visibleWindowTime;
        const scale = frontendLocalState.timeScale;
        const zoomPx = zoomedPositionPx - TRACK_SHELL_WIDTH;
        const newSpan = computeZoom(scale, span, 1 - zoomRatio, zoomPx);
        frontendLocalState.updateVisibleTime(newSpan);
        globals.rafScheduler.scheduleRedraw();
      },
      editSelection: (currentPx: number) => {
        return onTimeRangeBoundary(currentPx) !== null;
      },
      onSelection: (
          dragStartX: number,
          dragStartY: number,
          prevX: number,
          currentX: number,
          currentY: number,
          editing: boolean) => {
        const traceTime = globals.state.traceTime;
        const scale = frontendLocalState.timeScale;
        this.keepCurrentSelection = true;
        if (editing) {
          const selectedArea = frontendLocalState.selectedArea.area;
          if (selectedArea !== undefined) {
            let newTime = scale.pxToTime(currentX - TRACK_SHELL_WIDTH);
            // Have to check again for when one boundary crosses over the other.
            const curBoundary = onTimeRangeBoundary(prevX);
            if (curBoundary == null) return;
            const keepTime = curBoundary === 'START' ? selectedArea.endSec :
                                                       selectedArea.startSec;
            // Don't drag selection outside of current screen.
            if (newTime < keepTime) {
              newTime = Math.max(newTime, scale.pxToTime(scale.startPx));
            } else {
              newTime = Math.min(newTime, scale.pxToTime(scale.endPx));
            }
            frontendLocalState.selectArea(
                Math.max(Math.min(keepTime, newTime), traceTime.startSec),
                Math.min(Math.max(keepTime, newTime), traceTime.endSec),
            );
          }
        } else {
          const startPx = Math.max(
              Math.min(dragStartX, currentX) - TRACK_SHELL_WIDTH,
              scale.startPx);
          const endPx = Math.min(
              Math.max(dragStartX, currentX) - TRACK_SHELL_WIDTH, scale.endPx);
          frontendLocalState.selectArea(
              scale.pxToTime(startPx), scale.pxToTime(endPx));
          frontendLocalState.areaY.start = dragStartY;
          frontendLocalState.areaY.end = currentY;
        }
        globals.rafScheduler.scheduleRedraw();
      },
      selectingStarted: () => {
        globals.frontendLocalState.selectingArea = true;
      },
      selectingEnded: () => {
        globals.frontendLocalState.selectingArea = false;
        globals.frontendLocalState.areaY.start = undefined;
        globals.frontendLocalState.areaY.end = undefined;
        // Full redraw to color track shell.
        globals.rafScheduler.scheduleFullRedraw();
      }
    });
  }

  onremove() {
    window.removeEventListener('resize', this.onResize);
    if (this.zoomContent) this.zoomContent.shutdown();
  }

  view() {
    const scrollingPanels: AnyAttrsVnode[] = globals.state.scrollingTracks.map(
        id => m(TrackPanel, {key: id, id, selectable: true}));

    for (const group of Object.values(globals.state.trackGroups)) {
      scrollingPanels.push(m(TrackGroupPanel, {
        trackGroupId: group.id,
        key: `trackgroup-${group.id}`,
        selectable: true,
      }));
      if (group.collapsed) continue;
      for (const trackId of group.tracks) {
        scrollingPanels.push(m(TrackPanel, {
          key: `track-${group.id}-${trackId}`,
          id: trackId,
          selectable: true,
        }));
      }
    }
    scrollingPanels.unshift(m(QueryTable, {key: 'query'}));

    return m(
        '.page',
        m('.split-panel',
          m('.pan-and-zoom-content',
            {
              onclick: () => {
                // We don't want to deselect when panning/drag selecting.
                if (this.keepCurrentSelection) {
                  this.keepCurrentSelection = false;
                  return;
                }
                globals.makeSelection(Actions.deselect({}));
              }
            },
            m('.pinned-panel-container', m(PanelContainer, {
                doesScroll: false,
                panels: [
                  m(OverviewTimelinePanel, {key: 'overview'}),
                  m(TimeAxisPanel, {key: 'timeaxis'}),
                  m(TimeSelectionPanel, {key: 'timeselection'}),
                  m(NotesPanel, {key: 'notes'}),
                  m(TickmarkPanel, {key: 'searchTickmarks'}),
                  ...globals.state.pinnedTracks.map(
                      id => m(TrackPanel, {key: id, id, selectable: true})),
                ],
                kind: 'OVERVIEW',
              })),
            m('.scrolling-panel-container', m(PanelContainer, {
                doesScroll: true,
                panels: scrollingPanels,
                kind: 'TRACKS',
              }))),
          m('.video-panel',
            (globals.state.videoEnabled && globals.state.video != null) ?
                m(VideoPanel) :
                null)),
        m(DetailsPanel));
  }
}

export const ViewerPage = createPage({
  view() {
    return m(TraceViewer);
  }
});
