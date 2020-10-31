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

export type Column = (StringColumn|TimestampColumn|NumberColumn|StateColumn)&
    {title: string, columnId: string};

export interface StringColumn {
  kind: 'STRING';
  data: Uint16Array;
}

export interface TimestampColumn {
  kind: 'TIMESTAMP_NS';
  data: Float64Array;
}

export interface NumberColumn {
  kind: 'NUMBER';
  data: Uint16Array;
}

export interface StateColumn {
  kind: 'STATE';
  data: Uint16Array;
}

type TypedArrayConstructor = Uint16ArrayConstructor|Float64ArrayConstructor;
export interface ColumnDef {
  title: string;
  kind: string;
  columnConstructor: TypedArrayConstructor;
  columnId: string;
}

export interface AggregateData {
  tabName: string;
  columns: Column[];
  // For string interning.
  strings: string[];
}