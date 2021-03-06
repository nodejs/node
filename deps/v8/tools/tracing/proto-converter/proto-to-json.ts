// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as fs from 'fs';
import * as path from 'path';
import { Root } from 'protobufjs';

// Requirements: node 10.4.0+, npm

// Setup:
// (nvm is optional, you can also just install node manually)
// $ nvm use
// $ npm install
// $ npm run build

// Usage: node proto-to-json.js path_to_trace.proto input_file output_file

// Converts a binary proto file to a 'Trace Event Format' compatible .json file
// that can be used with chrome://tracing. Documentation of this format:
// https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU

// Attempts to reproduce the logic of the JSONTraceWriter in V8 in terms of the
// JSON fields it will include/exclude based on the data present in the trace
// event.

// TODO(petermarshall): Replace with Array#flat once it lands in Node.js.
const flatten = <T>(a: T[], b: T[]) => { a.push(...b); return a; }

// Convert a string representing an int or uint (64 bit) to a Number or throw
// if the value won't fit.
function parseIntOrThrow(int: string) {
  if (BigInt(int) > Number.MAX_SAFE_INTEGER) {
    throw new Error("Loss of int precision");
  }
  return Number(int);
}

function uint64AsHexString(val : string) : string {
  return "0x" + BigInt(val).toString(16);
}

function parseArgValue(arg: any) : any {
  if (arg.jsonValue) {
    return JSON.parse(arg.jsonValue);
  }
  if (typeof arg.stringValue !== 'undefined') {
    return arg.stringValue;
  }
  if (typeof arg.uintValue !== 'undefined') {
    return parseIntOrThrow(arg.uintValue);
  }
  if (typeof arg.intValue !== 'undefined') {
    return parseIntOrThrow(arg.intValue);
  }
  if (typeof arg.boolValue !== 'undefined') {
    return arg.boolValue;
  }
  if (typeof arg.doubleValue !== 'undefined') {
    // Handle [-]Infinity and NaN which protobufjs outputs as strings here.
    return typeof arg.doubleValue === 'string' ?
        arg.doubleValue : Number(arg.doubleValue);
  }
  if (typeof arg.pointerValue !== 'undefined') {
    return uint64AsHexString(arg.pointerValue);
  }
}

// These come from
// https://cs.chromium.org/chromium/src/base/trace_event/common/trace_event_common.h
const TRACE_EVENT_FLAG_HAS_ID: number = 1 << 1;
const TRACE_EVENT_FLAG_FLOW_IN: number = 1 << 8;
const TRACE_EVENT_FLAG_FLOW_OUT: number = 1 << 9;

async function main() {
  const root = new Root();
  const { resolvePath } = root;
  const numDirectoriesToStrip = 2;
  let initialOrigin: string|null;
  root.resolvePath = (origin, target) => {
    if (!origin) {
      initialOrigin = target;
      for (let i = 0; i <= numDirectoriesToStrip; i++) {
        initialOrigin = path.dirname(initialOrigin);
      }
      return resolvePath(origin, target);
    }
    return path.resolve(initialOrigin!, target);
  };
  const traceProto = await root.load(process.argv[2]);
  const Trace = traceProto.lookupType("Trace");
  const payload = await fs.promises.readFile(process.argv[3]);
  const msg = Trace.decode(payload).toJSON();
  const output = {
    traceEvents: msg.packet
      .filter((packet: any) => !!packet.chromeEvents)
      .map((packet: any) => packet.chromeEvents.traceEvents)
      .map((traceEvents: any) => traceEvents.map((e: any) => {

        const bind_id = (e.flags & (TRACE_EVENT_FLAG_FLOW_IN |
          TRACE_EVENT_FLAG_FLOW_OUT)) ? e.bindId : undefined;
        const scope = (e.flags & TRACE_EVENT_FLAG_HAS_ID) && e.scope ?
            e.scope : undefined;

        return {
          pid: e.processId,
          tid: e.threadId,
          ts: parseIntOrThrow(e.timestamp),
          tts: parseIntOrThrow(e.threadTimestamp),
          ph: String.fromCodePoint(e.phase),
          cat: e.categoryGroupName,
          name: e.name,
          dur: parseIntOrThrow(e.duration),
          tdur: parseIntOrThrow(e.threadDuration),
          bind_id: bind_id,
          flow_in: e.flags & TRACE_EVENT_FLAG_FLOW_IN ? true : undefined,
          flow_out: e.flags & TRACE_EVENT_FLAG_FLOW_OUT ? true : undefined,
          scope: scope,
          id: (e.flags & TRACE_EVENT_FLAG_HAS_ID) ?
              uint64AsHexString(e.id) : undefined,
          args: (e.args || []).reduce((js_args: any, proto_arg: any) => {
            js_args[proto_arg.name] = parseArgValue(proto_arg);
            return js_args;
          }, {})
        };
      }))
      .reduce(flatten, [])
  };
  await fs.promises.writeFile(process.argv[4], JSON.stringify(output, null, 2));
}

main().catch(console.error);
