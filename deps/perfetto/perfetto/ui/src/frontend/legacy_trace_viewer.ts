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
import {inflate} from 'pako';
import {assertTrue} from '../base/logging';
import {showModal} from './modal';

function readText(blob: Blob): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result === 'string') {
        return resolve(reader.result);
      }
    };
    reader.onerror = err => {
      reject(err);
    };
    reader.readAsText(blob);
  });
}

export async function isLegacyTrace(file: File): Promise<boolean> {
  const fileName = file.name.toLowerCase();
  if (fileName.endsWith('.json') || fileName.endsWith('.json.gz') ||
      fileName.endsWith('.zip') || fileName.endsWith('.ctrace') ||
      fileName.endsWith('.html')) {
    return true;
  }

  // Sometimes systrace formatted traces end with '.trace'. This is a
  // little generic to assume all such traces are systrace format though
  // so we read the beginning of the file and check to see if is has the
  // systrace header (several comment lines):
  if (fileName.endsWith('.trace')) {
    const header = await readText(file.slice(0, 512));
    const lines = header.split('\n');
    let commentCount = 0;
    for (const line of lines) {
      if (line.startsWith('#')) {
        commentCount++;
      }
    }
    if (commentCount > 5) {
      return true;
    }
  }

  return false;
}

export function openFileWithLegacyTraceViewer(file: File) {
  const reader = new FileReader();
  reader.onload = () => {
    if (reader.result instanceof ArrayBuffer) {
      return openBufferWithLegacyTraceViewer(
          file.name, reader.result, reader.result.byteLength);
    } else {
      const str = reader.result as string;
      return openBufferWithLegacyTraceViewer(file.name, str, str.length);
    }
  };
  reader.onerror = err => {
    console.error(err);
  };
  if (file.name.endsWith('.gz') || file.name.endsWith('.zip') ||
      file.name.endsWith('.ctrace')) {
    reader.readAsArrayBuffer(file);
  } else {
    reader.readAsText(file);
  }
}

export function openBufferWithLegacyTraceViewer(
    name: string, data: ArrayBuffer|string, size: number) {
  if (data instanceof ArrayBuffer) {
    assertTrue(size <= data.byteLength);
    if (size !== data.byteLength) {
      data = data.slice(0, size);
    }

    // Handle .ctrace files.
    const enc = new TextDecoder('utf-8');
    const header = enc.decode(data.slice(0, 7));
    if (header === 'TRACE:\n') {
      data = inflate(new Uint8Array(data.slice(7, size)), {to: 'string'});
    }
  }

  // The location.pathname mangling is to make this code work also when hosted
  // in a non-root sub-directory, for the case of CI artifacts.
  const urlParts = location.pathname.split('/');
  urlParts[urlParts.length - 1] = 'assets/catapult_trace_viewer.html';
  const catapultUrl = urlParts.join('/');
  const newWin = window.open(catapultUrl) as Window;
  if (newWin) {
    // Popup succeedeed.
    newWin.addEventListener('load', (e: Event) => {
      const doc = (e.target as Document);
      const ctl = doc.querySelector('x-profiling-view') as TraceViewerAPI;
      ctl.setActiveTrace(name, data);
    });
    return;
  }

  // Popup blocker detected.
  showModal({
    title: 'Open trace in the legacy Catapult Trace Viewer',
    content: m(
        'div',
        m('div', 'You are seeing this interstitial because popups are blocked'),
        m('div', 'Enable popups to skip this dialog next time.')),
    buttons: [{
      text: 'Open legacy UI',
      primary: true,
      id: 'open_legacy',
      action: () => openBufferWithLegacyTraceViewer(name, data, size),
    }],
  });
}

// TraceViewer method that we wire up to trigger the file load.
interface TraceViewerAPI extends Element {
  setActiveTrace(name: string, data: ArrayBuffer|string): void;
}
