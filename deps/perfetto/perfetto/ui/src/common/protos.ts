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

import {assertFalse, assertTrue} from '../base/logging';
import * as protos from '../gen/protos';

// Aliases protos to avoid the super nested namespaces.
// See https://www.typescriptlang.org/docs/handbook/namespaces.html#aliases
import AndroidLogConfig = protos.perfetto.protos.AndroidLogConfig;
import AndroidPowerConfig = protos.perfetto.protos.AndroidPowerConfig;
import AndroidLogId = protos.perfetto.protos.AndroidLogId;
import BatteryCounters =
    protos.perfetto.protos.AndroidPowerConfig.BatteryCounters;
import BufferConfig = protos.perfetto.protos.TraceConfig.BufferConfig;
import ChromeConfig = protos.perfetto.protos.ChromeConfig;
import ConsumerPort = protos.perfetto.protos.ConsumerPort;
import NativeContinuousDumpConfig =
    protos.perfetto.protos.HeapprofdConfig.ContinuousDumpConfig;
import JavaContinuousDumpConfig =
    protos.perfetto.protos.JavaHprofConfig.ContinuousDumpConfig;
import DataSourceConfig = protos.perfetto.protos.DataSourceConfig;
import FtraceConfig = protos.perfetto.protos.FtraceConfig;
import HeapprofdConfig = protos.perfetto.protos.HeapprofdConfig;
import JavaHprofConfig = protos.perfetto.protos.JavaHprofConfig;
import IAndroidPowerConfig = protos.perfetto.protos.IAndroidPowerConfig;
import IBufferConfig = protos.perfetto.protos.TraceConfig.IBufferConfig;
import IProcessStatsConfig = protos.perfetto.protos.IProcessStatsConfig;
import ISysStatsConfig = protos.perfetto.protos.ISysStatsConfig;
import ITraceConfig = protos.perfetto.protos.ITraceConfig;
import MeminfoCounters = protos.perfetto.protos.MeminfoCounters;
import ProcessStatsConfig = protos.perfetto.protos.ProcessStatsConfig;
import StatCounters = protos.perfetto.protos.SysStatsConfig.StatCounters;
import SysStatsConfig = protos.perfetto.protos.SysStatsConfig;
import TraceConfig = protos.perfetto.protos.TraceConfig;
import VmstatCounters = protos.perfetto.protos.VmstatCounters;

// Trace Processor protos.
import IRawQueryArgs = protos.perfetto.protos.IRawQueryArgs;
import RawQueryArgs = protos.perfetto.protos.RawQueryArgs;
import RawQueryResult = protos.perfetto.protos.RawQueryResult;
import StatusResult = protos.perfetto.protos.StatusResult;
import ComputeMetricArgs = protos.perfetto.protos.ComputeMetricArgs;
import ComputeMetricResult = protos.perfetto.protos.ComputeMetricResult;

// TODO(hjd): Maybe these should go in their own file.
export interface Row { [key: string]: number|string; }

const COLUMN_TYPE_STR = RawQueryResult.ColumnDesc.Type.STRING;
const COLUMN_TYPE_DOUBLE = RawQueryResult.ColumnDesc.Type.DOUBLE;
const COLUMN_TYPE_LONG = RawQueryResult.ColumnDesc.Type.LONG;

function getCell(result: RawQueryResult, column: number, row: number): number|
    string|null {
  const values = result.columns[column];
  if (values.isNulls![row]) return null;
  switch (result.columnDescriptors[column].type) {
    case COLUMN_TYPE_LONG:
      return +values.longValues![row];
    case COLUMN_TYPE_DOUBLE:
      return +values.doubleValues![row];
    case COLUMN_TYPE_STR:
      return values.stringValues![row];
    default:
      throw new Error('Unhandled type!');
  }
}

export function rawQueryResultColumns(result: RawQueryResult): string[] {
  // Two columns can conflict on the same name, e.g.
  // select x.foo, y.foo from x join y. In that case store them using the
  // full table.column notation.
  const res = [] as string[];
  const uniqColNames = new Set<string>();
  const colNamesToDedupe = new Set<string>();
  for (const col of result.columnDescriptors) {
    const colName = col.name || '';
    if (uniqColNames.has(colName)) {
      colNamesToDedupe.add(colName);
    }
    uniqColNames.add(colName);
  }
  for (let i = 0; i < result.columnDescriptors.length; i++) {
    const colName = result.columnDescriptors[i].name || '';
    if (colNamesToDedupe.has(colName)) {
      res.push(`${colName}.${i + 1}`);
    } else {
      res.push(colName);
    }
  }
  return res;
}

export function* rawQueryResultIter(result: RawQueryResult) {
  const columns: Array<[string, number]> = rawQueryResultColumns(result).map(
      (name, i): [string, number] => [name, i]);
  for (let rowNum = 0; rowNum < result.numRecords; rowNum++) {
    const row: Row = {};
    for (const [name, colNum] of columns) {
      const cell = getCell(result, colNum, rowNum);
      row[name] = cell === null ? '[NULL]' : cell;
    }
    yield row;
  }
}

export const NUM = 0;
export const STR = 'str';
export const NUM_NULL: number|null = 1;
export const STR_NULL: string|null = 'str_null';

/**
 * This function allows for type safe use of RawQueryResults.
 * The input is a RawQueryResult (|raw|) and a "spec".
 * A spec is an object where the keys are column names and the values
 * are constants representing the types. For example:
 * {
 *   upid: NUM,
 *   pid: NUM_NULL,
 *   processName: STR_NULL,
 * }
 * The output is a iterable of rows each row looks like the given spec:
 * {
 *   upid: 1,
 *   pid: 42,
 *   processName: null,
 * }
 * Each row has an appropriate typescript type based on the spec so there
 * is no need to use ! or cast when using the result of rawQueryToRows.
 * Note: type checking to ensure that the RawQueryResult matches the spec
 * happens at runtime, so if a query can return null and this is not reflected
 * in the spec this will still crash at runtime.
 */
export function*
    rawQueryToRows<T>(raw: RawQueryResult, spec: T): IterableIterator<T> {
  const allColumns = rawQueryResultColumns(raw);
  const columns: Array<[string, (row: number) => string | number | null]> = [];
  for (const [key, columnSpec] of Object.entries(spec)) {
    const i = allColumns.indexOf(key);
    assertTrue(i !== -1, `Expected column "${key}" (cols ${allColumns})`);

    const column = raw.columns[i];
    const isNulls = column.isNulls!;
    const columnType = raw.columnDescriptors[i].type;

    if (columnSpec === NUM || columnSpec === STR) {
      for (let j = 0; j < raw.numRecords; j++) {
        assertFalse(column.isNulls![i], `Unexpected null in ${key} row ${j}`);
      }
    }

    if (columnSpec === NUM || columnSpec === NUM_NULL) {
      if (columnType === COLUMN_TYPE_STR) {
        throw new Error(`Expected numbers in column ${key} found strings`);
      }
    } else if (columnSpec === STR || columnSpec === STR_NULL) {
      if (columnType === COLUMN_TYPE_LONG ||
          columnType === COLUMN_TYPE_DOUBLE) {
        throw new Error(`Expected strings in column ${key} found numbers`);
      }
    }

    let accessor;
    switch (columnType) {
      case COLUMN_TYPE_LONG: {
        const values = column.longValues!;
        accessor = (i: number) => isNulls[i] ? null : +values[i];
        break;
      }
      case COLUMN_TYPE_DOUBLE: {
        const values = column.doubleValues!;
        accessor = (i: number) => isNulls[i] ? null : values[i];
        break;
      }
      case COLUMN_TYPE_STR: {
        const values = column.stringValues!;
        accessor = (i: number) => isNulls[i] ? null : values[i];
        break;
      }
      default:
        // We can only reach here if the column is completely null.
        accessor = (_: number) => null;
        break;
    }
    columns.push([key, accessor]);
  }

  for (let i = 0; i < raw.numRecords; i++) {
    const row: {[_: string]: number | string | null} = {};
    for (const [name, accessor] of columns) {
      row[name] = accessor(i);
    }
    yield row as {} as T;
  }
}

export {
  AndroidLogConfig,
  AndroidLogId,
  AndroidPowerConfig,
  BatteryCounters,
  BufferConfig,
  ChromeConfig,
  ConsumerPort,
  ComputeMetricArgs,
  ComputeMetricResult,
  DataSourceConfig,
  FtraceConfig,
  HeapprofdConfig,
  IAndroidPowerConfig,
  IBufferConfig,
  IProcessStatsConfig,
  IRawQueryArgs,
  ISysStatsConfig,
  ITraceConfig,
  JavaContinuousDumpConfig,
  JavaHprofConfig,
  MeminfoCounters,
  NativeContinuousDumpConfig,
  ProcessStatsConfig,
  RawQueryArgs,
  RawQueryResult,
  StatCounters,
  StatusResult,
  SysStatsConfig,
  TraceConfig,
  VmstatCounters,
};
