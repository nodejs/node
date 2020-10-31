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

import * as protobuf from 'protobufjs/minimal';

import {perfetto} from '../gen/protos';

import {AdbAuthState, AdbBaseConsumerPort} from './adb_base_controller';
import {Adb, AdbStream} from './adb_interfaces';
import {
  isReadBuffersResponse,
} from './consumer_port_types';
import {Consumer} from './record_controller_interfaces';

enum SocketState {
  DISCONNECTED,
  BINDING_IN_PROGRESS,
  BOUND,
}

// See wire_protocol.proto for more details.
const WIRE_PROTOCOL_HEADER_SIZE = 4;
const MAX_IPC_BUFFER_SIZE = 128 * 1024;

const PROTO_LEN_DELIMITED_WIRE_TYPE = 2;
const TRACE_PACKET_PROTO_ID = 1;
const TRACE_PACKET_PROTO_TAG =
    (TRACE_PACKET_PROTO_ID << 3) | PROTO_LEN_DELIMITED_WIRE_TYPE;

declare type Frame = perfetto.protos.IPCFrame;
declare type IMethodInfo =
    perfetto.protos.IPCFrame.BindServiceReply.IMethodInfo;
declare type ISlice = perfetto.protos.ReadBuffersResponse.ISlice;

interface Command {
  method: string;
  params: Uint8Array;
}

const TRACED_SOCKET = '/dev/socket/traced_consumer';

export class AdbSocketConsumerPort extends AdbBaseConsumerPort {
  private socketState = SocketState.DISCONNECTED;

  private socket?: AdbStream;
  // Wire protocol request ID. After each request it is increased. It is needed
  // to keep track of the type of request, and parse the response correctly.
  private requestId = 1;

  // Buffers received wire protocol data.
  private incomingBuffer = new Uint8Array(MAX_IPC_BUFFER_SIZE);
  private incomingBufferLen = 0;
  private frameToParseLen = 0;

  private availableMethods: IMethodInfo[] = [];
  private serviceId = -1;

  private resolveBindingPromise!: VoidFunction;
  private requestMethods = new Map<number, string>();

  // Needed for ReadBufferResponse: all the trace packets are split into
  // several slices. |partialPacket| is the buffer for them. Once we receive a
  // slice with the flag |lastSliceForPacket|, a new packet is created.
  private partialPacket: ISlice[] = [];
  // Accumulates trace packets into a proto trace file..
  private traceProtoWriter = protobuf.Writer.create();

  private socketCommandQueue: Command[] = [];

  constructor(adb: Adb, consumer: Consumer) {
    super(adb, consumer);
  }

  async invoke(method: string, params: Uint8Array) {
    // ADB connection & authentication is handled by the superclass.
    console.assert(this.state === AdbAuthState.CONNECTED);
    this.socketCommandQueue.push({method, params});

    if (this.socketState === SocketState.BINDING_IN_PROGRESS) return;
    if (this.socketState === SocketState.DISCONNECTED) {
      this.socketState = SocketState.BINDING_IN_PROGRESS;
      await this.listenForMessages();
      await this.bind();
      this.traceProtoWriter = protobuf.Writer.create();
      this.socketState = SocketState.BOUND;
    }

    console.assert(this.socketState === SocketState.BOUND);

    for (const cmd of this.socketCommandQueue) {
      this.invokeInternal(cmd.method, cmd.params);
    }
    this.socketCommandQueue = [];
  }

  private invokeInternal(method: string, argsProto: Uint8Array) {
    // Socket is bound in invoke().
    console.assert(this.socketState === SocketState.BOUND);
    const requestId = this.requestId++;
    const methodId = this.findMethodId(method);
    if (methodId === undefined) {
      // This can happen with 'GetTraceStats': it seems that not all the Android
      // <= 9 devices support it.
      console.error(`Method ${method} not supported by the target`);
      return;
    }
    const frame = new perfetto.protos.IPCFrame({
      requestId,
      msgInvokeMethod: new perfetto.protos.IPCFrame.InvokeMethod(
          {serviceId: this.serviceId, methodId, argsProto})
    });
    this.requestMethods.set(requestId, method);
    this.sendFrame(frame);

    if (method === 'EnableTracing') this.setDurationStatus(argsProto);
  }

  static generateFrameBufferToSend(frame: Frame): Uint8Array {
    const frameProto: Uint8Array =
        perfetto.protos.IPCFrame.encode(frame).finish();
    const frameLen = frameProto.length;
    const buf = new Uint8Array(WIRE_PROTOCOL_HEADER_SIZE + frameLen);
    const dv = new DataView(buf.buffer);
    dv.setUint32(0, frameProto.length, /* littleEndian */ true);
    for (let i = 0; i < frameLen; i++) {
      dv.setUint8(WIRE_PROTOCOL_HEADER_SIZE + i, frameProto[i]);
    }
    return buf;
  }

  async sendFrame(frame: Frame) {
    console.assert(this.socket !== undefined);
    if (!this.socket) return;
    const buf = AdbSocketConsumerPort.generateFrameBufferToSend(frame);
    await this.socket.write(buf);
  }

  async listenForMessages() {
    this.socket = await this.adb.socket(TRACED_SOCKET);
    this.socket.onData = (raw) => this.handleReceivedData(raw);
    this.socket.onClose = () => {
      this.socketState = SocketState.DISCONNECTED;
      this.socketCommandQueue = [];
    };
  }

  private parseMessageSize(buffer: Uint8Array) {
    const dv = new DataView(buffer.buffer, buffer.byteOffset, buffer.length);
    return dv.getUint32(0, true);
  }

  private parseMessage(frameBuffer: Uint8Array) {
    const frame = perfetto.protos.IPCFrame.decode(frameBuffer);
    this.handleIncomingFrame(frame);
  }

  private incompleteSizeHeader() {
    if (!this.frameToParseLen) {
      console.assert(this.incomingBufferLen < WIRE_PROTOCOL_HEADER_SIZE);
      return true;
    }
    return false;
  }

  private canCompleteSizeHeader(newData: Uint8Array) {
    return newData.length + this.incomingBufferLen > WIRE_PROTOCOL_HEADER_SIZE;
  }

  private canParseFullMessage(newData: Uint8Array) {
    return this.frameToParseLen &&
        this.incomingBufferLen + newData.length >= this.frameToParseLen;
  }

  private appendToIncomingBuffer(array: Uint8Array) {
    this.incomingBuffer.set(array, this.incomingBufferLen);
    this.incomingBufferLen += array.length;
  }

  handleReceivedData(newData: Uint8Array) {
    if (this.incompleteSizeHeader() && this.canCompleteSizeHeader(newData)) {
      const newDataBytesToRead =
          WIRE_PROTOCOL_HEADER_SIZE - this.incomingBufferLen;
      // Add to the incoming buffer the remaining bytes to arrive at
      // WIRE_PROTOCOL_HEADER_SIZE
      this.appendToIncomingBuffer(newData.subarray(0, newDataBytesToRead));
      newData = newData.subarray(newDataBytesToRead);

      this.frameToParseLen = this.parseMessageSize(this.incomingBuffer);
      this.incomingBufferLen = 0;
    }

    // Parse all complete messages in incomingBuffer and newData.
    while (this.canParseFullMessage(newData)) {
      // All the message is in the newData buffer.
      if (this.incomingBufferLen === 0) {
        this.parseMessage(newData.subarray(0, this.frameToParseLen));
        newData = newData.subarray(this.frameToParseLen);
      } else {  // We need to complete the local buffer.
        // Read the remaining part of this message.
        const bytesToCompleteMessage =
            this.frameToParseLen - this.incomingBufferLen;
        this.appendToIncomingBuffer(
            newData.subarray(0, bytesToCompleteMessage));
        this.parseMessage(
            this.incomingBuffer.subarray(0, this.frameToParseLen));
        this.incomingBufferLen = 0;
        // Remove the data just parsed.
        newData = newData.subarray(bytesToCompleteMessage);
      }
      this.frameToParseLen = 0;
      if (!this.canCompleteSizeHeader(newData)) break;

      this.frameToParseLen =
          this.parseMessageSize(newData.subarray(0, WIRE_PROTOCOL_HEADER_SIZE));
      newData = newData.subarray(WIRE_PROTOCOL_HEADER_SIZE);
    }
    // Buffer the remaining data (part of the next header + message).
    this.appendToIncomingBuffer(newData);
  }

  decodeResponse(
      requestId: number, responseProto: Uint8Array, hasMore = false) {
    const method = this.requestMethods.get(requestId);
    if (!method) {
      console.error(`Unknown request id: ${requestId}`);
      this.sendErrorMessage(`Wire protocol error.`);
      return;
    }
    const decoder = decoders.get(method);
    if (decoder === undefined) {
      console.error(`Unable to decode method: ${method}`);
      return;
    }
    const decodedResponse = decoder(responseProto);
    const response = {type: `${method}Response`, ...decodedResponse};

    // TODO(nicomazz): Fix this.
    // We assemble all the trace and then send it back to the main controller.
    // This is a temporary solution, that will be changed in a following CL,
    // because now both the chrome consumer port and the other adb consumer port
    // send back the entire trace, while the correct behavior should be to send
    // back the slices, that are assembled by the main record controller.
    if (isReadBuffersResponse(response)) {
      if (response.slices) this.handleSlices(response.slices);
      if (!hasMore) this.sendReadBufferResponse();
      return;
    }
    this.sendMessage(response);
  }

  handleSlices(slices: ISlice[]) {
    for (const slice of slices) {
      this.partialPacket.push(slice);
      if (slice.lastSliceForPacket) {
        const tracePacket = this.generateTracePacket(this.partialPacket);
        this.traceProtoWriter.uint32(TRACE_PACKET_PROTO_TAG);
        this.traceProtoWriter.bytes(tracePacket);
        this.partialPacket = [];
      }
    }
  }

  generateTracePacket(slices: ISlice[]): Uint8Array {
    let bufferSize = 0;
    for (const slice of slices) bufferSize += slice.data!.length;
    const fullBuffer = new Uint8Array(bufferSize);
    let written = 0;
    for (const slice of slices) {
      const data = slice.data!;
      fullBuffer.set(data, written);
      written += data.length;
    }
    return fullBuffer;
  }

  sendReadBufferResponse() {
    this.sendMessage(this.generateChunkReadResponse(
        this.traceProtoWriter.finish(), /* last */ true));
  }

  bind() {
    console.assert(this.socket !== undefined);
    const requestId = this.requestId++;
    const frame = new perfetto.protos.IPCFrame({
      requestId,
      msgBindService: new perfetto.protos.IPCFrame.BindService(
          {serviceName: 'ConsumerPort'})
    });
    return new Promise((resolve, _) => {
      this.resolveBindingPromise = resolve;
      this.sendFrame(frame);
    });
  }

  findMethodId(method: string): number|undefined {
    const methodObject = this.availableMethods.find((m) => m.name === method);
    if (methodObject && methodObject.id) return methodObject.id;
    return undefined;
  }

  static async hasSocketAccess(device: USBDevice, adb: Adb): Promise<boolean> {
    await adb.connect(device);
    try {
      const socket = await adb.socket(TRACED_SOCKET);
      socket.close();
      return true;
    } catch (e) {
      return false;
    }
  }

  handleIncomingFrame(frame: perfetto.protos.IPCFrame) {
    const requestId = frame.requestId as number;
    switch (frame.msg) {
      case 'msgBindServiceReply': {
        const msgBindServiceReply = frame.msgBindServiceReply;
        if (msgBindServiceReply && msgBindServiceReply.methods &&
            msgBindServiceReply.serviceId) {
          console.assert(msgBindServiceReply.success);
          this.availableMethods = msgBindServiceReply.methods;
          this.serviceId = msgBindServiceReply.serviceId;
          this.resolveBindingPromise();
          this.resolveBindingPromise = () => {};
        }
        return;
      }
      case 'msgInvokeMethodReply': {
        const msgInvokeMethodReply = frame.msgInvokeMethodReply;
        if (msgInvokeMethodReply && msgInvokeMethodReply.replyProto) {
          if (!msgInvokeMethodReply.success) {
            console.error(
                'Unsuccessful method invocation: ', msgInvokeMethodReply);
            return;
          }
          this.decodeResponse(
              requestId,
              msgInvokeMethodReply.replyProto,
              msgInvokeMethodReply.hasMore === true);
        }
        return;
      }
      default:
        console.error(`not recognized frame message: ${frame.msg}`);
    }
  }
}

const decoders =
    new Map<string, Function>()
        .set('EnableTracing', perfetto.protos.EnableTracingResponse.decode)
        .set('FreeBuffers', perfetto.protos.FreeBuffersResponse.decode)
        .set('ReadBuffers', perfetto.protos.ReadBuffersResponse.decode)
        .set('DisableTracing', perfetto.protos.DisableTracingResponse.decode)
        .set('GetTraceStats', perfetto.protos.GetTraceStatsResponse.decode);