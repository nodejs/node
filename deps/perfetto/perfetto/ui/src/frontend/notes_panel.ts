// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
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

import {Actions} from '../common/actions';
import {AreaNote, Note} from '../common/state';
import {timeToString} from '../common/time';

import {randomColor} from './colorizer';
import {TRACK_SHELL_WIDTH} from './css_constants';
import {globals} from './globals';
import {gridlines} from './gridline_helper';
import {Panel, PanelSize} from './panel';

const FLAG_WIDTH = 16;
const AREA_TRIANGLE_WIDTH = 10;
const MOVIE_WIDTH = 16;
const FLAG = `\uE153`;
const MOVIE = '\uE8DA';

function toSummary(s: string) {
  const newlineIndex = s.indexOf('\n') > 0 ? s.indexOf('\n') : s.length;
  return s.slice(0, Math.min(newlineIndex, s.length, 16));
}

export class NotesPanel extends Panel {
  hoveredX: null|number = null;

  oncreate({dom}: m.CVnodeDOM) {
    dom.addEventListener('mousemove', (e: Event) => {
      this.hoveredX = (e as MouseEvent).layerX - TRACK_SHELL_WIDTH;
      if (globals.state.scrubbingEnabled) {
        const timescale = globals.frontendLocalState.timeScale;
        const timestamp = timescale.pxToTime(this.hoveredX);
        globals.frontendLocalState.setVidTimestamp(timestamp);
      }
      globals.rafScheduler.scheduleRedraw();
    }, {passive: true});
    dom.addEventListener('mouseenter', (e: Event) => {
      this.hoveredX = (e as MouseEvent).layerX - TRACK_SHELL_WIDTH;
      globals.rafScheduler.scheduleRedraw();
    });
    dom.addEventListener('mouseout', () => {
      this.hoveredX = null;
      globals.frontendLocalState.setHoveredNoteTimestamp(-1);
      globals.rafScheduler.scheduleRedraw();
    }, {passive: true});
  }

  view() {
    return m(
      '.notes-panel',
      {
        onclick: (e: MouseEvent) => {
          const isMovie = globals.state.flagPauseEnabled;
          this.onClick(e.layerX - TRACK_SHELL_WIDTH, e.layerY, isMovie);
          e.stopPropagation();
        },
      });
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const timeScale = globals.frontendLocalState.timeScale;
    const range = globals.frontendLocalState.visibleWindowTime;
    let aNoteIsHovered = false;

    ctx.fillStyle = '#999';
    ctx.fillRect(TRACK_SHELL_WIDTH - 2, 0, 2, size.height);
    for (const xAndTime of gridlines(size.width, range, timeScale)) {
      ctx.fillRect(xAndTime[0], 0, 1, size.height);
    }

    ctx.textBaseline = 'bottom';
    ctx.font = '10px Helvetica';

    for (const note of Object.values(globals.state.notes)) {
      const timestamp = note.timestamp;
      if ((note.noteType !== 'AREA' && !timeScale.timeInBounds(timestamp)) ||
          (note.noteType === 'AREA' &&
           !timeScale.timeInBounds(note.area.endSec) &&
           !timeScale.timeInBounds(note.area.startSec))) {
        continue;
      }
      const currentIsHovered =
          this.hoveredX && this.mouseOverNote(this.hoveredX, note);
      if (currentIsHovered) aNoteIsHovered = true;

      const selection = globals.state.currentSelection;
      const isSelected = selection !== null && selection.kind === 'NOTE' &&
          selection.id === note.id;
      const x = timeScale.timeToPx(timestamp);
      const left = Math.floor(x + TRACK_SHELL_WIDTH);

      // Draw flag or marker.
      if (note.noteType === 'AREA') {
        this.drawAreaMarker(
            ctx,
            left,
            Math.floor(
                timeScale.timeToPx(note.area.endSec) + TRACK_SHELL_WIDTH),
            note.color,
            isSelected);
      } else {
        this.drawFlag(
            ctx, left, size.height, note.color, note.noteType, isSelected);
      }

      if (note.text) {
        const summary = toSummary(note.text);
        const measured = ctx.measureText(summary);
        // Add a white semi-transparent background for the text.
        ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
        ctx.fillRect(
            left + FLAG_WIDTH + 2, size.height + 2, measured.width + 2, -12);
        ctx.fillStyle = '#3c4b5d';
        ctx.fillText(summary, left + FLAG_WIDTH + 3, size.height + 1);
      }
    }

    // A real note is hovered so we don't need to see the preview line.
    // TODO(taylori): Change cursor to pointer here.
    if (aNoteIsHovered) globals.frontendLocalState.setHoveredNoteTimestamp(-1);

    // View preview note flag when hovering on notes panel.
    if (!aNoteIsHovered && this.hoveredX !== null) {
      const timestamp = timeScale.pxToTime(this.hoveredX);
      if (timeScale.timeInBounds(timestamp)) {
        globals.frontendLocalState.setHoveredNoteTimestamp(timestamp);
        const x = timeScale.timeToPx(timestamp);
        const left = Math.floor(x + TRACK_SHELL_WIDTH);
        this.drawFlag(
            ctx, left, size.height, '#aaa', 'DEFAULT', /* fill */ true);
      }
    }
  }

  private drawAreaMarker(
      ctx: CanvasRenderingContext2D, x: number, xEnd: number, color: string,
      fill: boolean) {
    ctx.fillStyle = color;
    ctx.strokeStyle = color;
    const topOffset = 10;
    // Don't draw in the track shell section.
    if (x >= globals.frontendLocalState.timeScale.startPx + TRACK_SHELL_WIDTH) {
      // Draw left triangle.
      ctx.beginPath();
      ctx.moveTo(x, topOffset);
      ctx.lineTo(x, topOffset + AREA_TRIANGLE_WIDTH);
      ctx.lineTo(x + AREA_TRIANGLE_WIDTH, topOffset);
      ctx.lineTo(x, topOffset);
      if (fill) ctx.fill();
      ctx.stroke();
    }
    // Draw right triangle.
    ctx.beginPath();
    ctx.moveTo(xEnd, topOffset);
    ctx.lineTo(xEnd, topOffset + AREA_TRIANGLE_WIDTH);
    ctx.lineTo(xEnd - AREA_TRIANGLE_WIDTH, topOffset);
    ctx.lineTo(xEnd, topOffset);
    if (fill) ctx.fill();
    ctx.stroke();

    // Start line after track shell section, join triangles.
    const startDraw =
        Math.max(
            x,
            globals.frontendLocalState.timeScale.startPx + TRACK_SHELL_WIDTH) -
        1;
    ctx.fillRect(startDraw, topOffset - 1, xEnd - startDraw + 1, 1);
  }

  private drawFlag(
      ctx: CanvasRenderingContext2D, x: number, height: number, color: string,
      noteType: 'DEFAULT'|'AREA'|'MOVIE', fill?: boolean) {
    const prevFont = ctx.font;
    const prevBaseline = ctx.textBaseline;
    ctx.textBaseline = 'alphabetic';
    // Adjust height for icon font.
    ctx.font = '24px Material Icons';
    ctx.fillStyle = color;
    ctx.strokeStyle = color;
    // The ligatures have padding included that means the icon is not drawn
    // exactly at the x value. This adjusts for that.
    const iconPadding = 6;
    if (fill) {
      ctx.fillText(
          noteType === 'MOVIE' ? MOVIE : FLAG, x - iconPadding, height + 2);
    } else {
      ctx.strokeText(
          noteType === 'MOVIE' ? MOVIE : FLAG, x - iconPadding, height + 2.5);
    }
    ctx.font = prevFont;
    ctx.textBaseline = prevBaseline;
  }


  private onClick(x: number, _: number, isMovie: boolean) {
    const timeScale = globals.frontendLocalState.timeScale;
    const timestamp = timeScale.pxToTime(x);
    for (const note of Object.values(globals.state.notes)) {
      if (this.hoveredX && this.mouseOverNote(this.hoveredX, note)) {
        if (note.noteType === 'MOVIE') {
          globals.frontendLocalState.setVidTimestamp(note.timestamp);
        } else if (note.noteType === 'AREA') {
          globals.frontendLocalState.selectArea(
              note.area.startSec, note.area.endSec, note.area.tracks);
        }
        globals.makeSelection(Actions.selectNote({id: note.id}));
        return;
      }
    }
    if (isMovie) {
      globals.frontendLocalState.setVidTimestamp(timestamp);
    }
    const color = randomColor();
    globals.makeSelection(Actions.addNote({timestamp, color, isMovie}));
  }

  private mouseOverNote(x: number, note: AreaNote|Note): boolean {
    const timeScale = globals.frontendLocalState.timeScale;
    const noteX = timeScale.timeToPx(note.timestamp);
    if (note.noteType === 'AREA') {
      return (noteX <= x && x < noteX + AREA_TRIANGLE_WIDTH) ||
          (timeScale.timeToPx(note.area.endSec) > x &&
           x > timeScale.timeToPx(note.area.endSec) - AREA_TRIANGLE_WIDTH);
    } else {
      const width = (note.noteType === 'MOVIE') ? MOVIE_WIDTH : FLAG_WIDTH;
      return noteX <= x && x < noteX + width;
    }
  }
}

interface NotesEditorPanelAttrs {
  id: string;
}

export class NotesEditorPanel extends Panel<NotesEditorPanelAttrs> {
  view({attrs}: m.CVnode<NotesEditorPanelAttrs>) {
    const note = globals.state.notes[attrs.id];
    const startTime = note.timestamp - globals.state.traceTime.startSec;
    return m(
        '.notes-editor-panel',
        m('.notes-editor-panel-heading-bar',
          m('.notes-editor-panel-heading',
            `Annotation at ${timeToString(startTime)}`),
          m('input[type=text]', {
            onkeydown: (e: Event) => {
              e.stopImmediatePropagation();
            },
            value: note.text,
            onchange: m.withAttr(
                'value',
                newText => {
                  globals.dispatch(Actions.changeNoteText({
                    id: attrs.id,
                    newText,
                  }));
                }),
          }),
          m('span.color-change', `Change color: `, m('input[type=color]', {
              value: note.color,
              onchange: m.withAttr(
                  'value',
                  newColor => {
                    globals.dispatch(Actions.changeNoteColor({
                      id: attrs.id,
                      newColor,
                    }));
                  }),
            })),
          m('button',
            {
              onclick: () => {
                globals.dispatch(Actions.removeNote({id: attrs.id}));
                globals.frontendLocalState.currentTab = undefined;
                globals.rafScheduler.scheduleFullRedraw();
              }
            },
            'Remove')),
    );
  }

  renderCanvas(_ctx: CanvasRenderingContext2D, _size: PanelSize) {}
}
