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

import {Engine} from '../common/engine';
import {
  RawQueryResult,
  rawQueryResultColumns,
  rawQueryResultIter
} from '../common/protos';
import {
  createWasmEngine,
  destroyWasmEngine,
  warmupWasmEngine,
  WasmEngineProxy
} from '../common/wasm_engine_proxy';

const kEngineId = 'engine';
const kSliceSize = 1024 * 1024;


interface OnReadSlice {
  (blob: Blob, end: number, slice: ArrayBuffer): void;
}

function readSlice(
    blob: Blob, start: number, end: number, callback: OnReadSlice) {
  const slice = blob.slice(start, end);
  const reader = new FileReader();
  reader.onerror = e => {
    console.error(e);
  };
  reader.onloadend = _ => {
    callback(blob, end, reader.result as ArrayBuffer);
  };
  reader.readAsArrayBuffer(slice);
}


// Represents an in flight or resolved query.
type QueryState = QueryPendingState|QueryResultState|QueryErrorState;

interface QueryResultState {
  kind: 'QueryResultState';
  id: number;
  query: string;
  result: RawQueryResult;
  executionTimeNs: number;
}

interface QueryErrorState {
  kind: 'QueryErrorState';
  id: number;
  query: string;
  error: string;
}

interface QueryPendingState {
  kind: 'QueryPendingState';
  id: number;
  query: string;
}

function isPending(q: QueryState): q is QueryPendingState {
  return q.kind === 'QueryPendingState';
}

function isError(q: QueryState): q is QueryErrorState {
  return q.kind === 'QueryErrorState';
}

function isResult(q: QueryState): q is QueryResultState {
  return q.kind === 'QueryResultState';
}


// Helpers for accessing a query result
function columns(result: RawQueryResult): string[] {
  return [...rawQueryResultColumns(result)];
}

function rows(result: RawQueryResult, offset: number, count: number):
    Array<Array<number|string>> {
  const rows: Array<Array<number|string>> = [];

  let i = 0;
  for (const value of rawQueryResultIter(result)) {
    if (i < offset) continue;
    if (i > offset + count) break;
    rows.push(Object.values(value));
    i++;
  }
  return rows;
}


// State machine controller for the UI.
type Input = NewFile|NewQuery|MoreData|QuerySuccess|QueryFailure;

interface NewFile {
  kind: 'NewFile';
  file: File;
}

interface MoreData {
  kind: 'MoreData';
  end: number;
  source: Blob;
  buffer: ArrayBuffer;
}

interface NewQuery {
  kind: 'NewQuery';
  query: string;
}

interface QuerySuccess {
  kind: 'QuerySuccess';
  id: number;
  result: RawQueryResult;
}

interface QueryFailure {
  kind: 'QueryFailure';
  id: number;
  error: string;
}

class QueryController {
  engine: Engine|undefined;
  file: File|undefined;
  state: 'initial'|'loading'|'ready';
  render: (state: QueryController) => void;
  nextQueryId: number;
  queries: Map<number, QueryState>;

  constructor(render: (state: QueryController) => void) {
    this.render = render;
    this.state = 'initial';
    this.nextQueryId = 0;
    this.queries = new Map();
    this.render(this);
  }

  onInput(input: Input) {
    // tslint:disable-next-line no-any
    const f = (this as any)[`${this.state}On${input.kind}`];
    if (f === undefined) {
      throw new Error(`No edge for input '${input.kind}' in '${this.state}'`);
    }
    f.call(this, input);
    this.render(this);
  }

  initialOnNewFile(input: NewFile) {
    this.state = 'loading';
    if (this.engine) {
      destroyWasmEngine(kEngineId);
    }
    this.engine = new WasmEngineProxy('engine', createWasmEngine(kEngineId));

    this.file = input.file;
    this.readNextSlice(0);
  }

  loadingOnMoreData(input: MoreData) {
    if (input.source !== this.file) return;
    this.engine!.parse(new Uint8Array(input.buffer));
    if (input.end === this.file.size) {
      this.engine!.notifyEof();
      this.state = 'ready';
    } else {
      this.readNextSlice(input.end);
    }
  }

  readyOnNewQuery(input: NewQuery) {
    const id = this.nextQueryId++;
    this.queries.set(id, {
      kind: 'QueryPendingState',
      id,
      query: input.query,
    });

    this.engine!.query(input.query)
        .then(result => {
          if (result.error) {
            this.onInput({
              kind: 'QueryFailure',
              id,
              error: result.error,
            });
          } else {
            this.onInput({
              kind: 'QuerySuccess',
              id,
              result,
            });
          }
        })
        .catch(error => {
          this.onInput({
            kind: 'QueryFailure',
            id,
            error,
          });
        });
  }

  readyOnQuerySuccess(input: QuerySuccess) {
    const oldQueryState = this.queries.get(input.id);
    console.log('sucess', input);
    if (!oldQueryState) return;
    this.queries.set(input.id, {
      kind: 'QueryResultState',
      id: oldQueryState.id,
      query: oldQueryState.query,
      result: input.result,
      executionTimeNs: +input.result.executionTimeNs,
    });
  }

  readyOnQueryFailure(input: QueryFailure) {
    const oldQueryState = this.queries.get(input.id);
    console.log('failure', input);
    if (!oldQueryState) return;
    this.queries.set(input.id, {
      kind: 'QueryErrorState',
      id: oldQueryState.id,
      query: oldQueryState.query,
      error: input.error,
    });
  }

  readNextSlice(start: number) {
    const end = Math.min(this.file!.size, start + kSliceSize);
    readSlice(this.file!, start, end, (source, end, buffer) => {
      this.onInput({
        kind: 'MoreData',
        end,
        source,
        buffer,
      });
    });
  }
}

function render(root: Element, controller: QueryController) {
  const queries = [...controller.queries.values()].sort((a, b) => b.id - a.id);
  m.render(root, [
    m('h1', controller.state),
    m('input[type=file]', {
      onchange: (e: Event) => {
        if (!(e.target instanceof HTMLInputElement)) return;
        if (!e.target.files) return;
        if (!e.target.files[0]) return;
        const file = e.target.files[0];
        controller.onInput({
          kind: 'NewFile',
          file,
        });
      },
    }),
    m('input[type=text]', {
      disabled: controller.state !== 'ready',
      onchange: (e: Event) => {
        controller.onInput({
          kind: 'NewQuery',
          query: (e.target as HTMLInputElement).value,
        });
      }
    }),
    m('.query-list',
      queries.map(
          q =>
              m('.query',
                {
                  key: q.id,
                },
                m('.query-text', q.query),
                m('.query-time',
                  isResult(q) ? `${q.executionTimeNs / 1000000}ms` : ''),
                isResult(q) ? m('.query-content', renderTable(q.result)) : null,
                isError(q) ? m('.query-content', q.error) : null,
                isPending(q) ? m('.query-content') : null))),
  ]);
}

function renderTable(result: RawQueryResult) {
  return m(
      'table',
      m('tr', columns(result).map(c => m('th', c))),
      rows(result, 0, 1000).map(r => {
        return m('tr', Object.values(r).map(d => m('td', d)));
      }));
}

function main() {
  warmupWasmEngine();
  const root = document.querySelector('#root');
  if (!root) throw new Error('Could not find root element');
  new QueryController(ctrl => render(root, ctrl));
}

main();
