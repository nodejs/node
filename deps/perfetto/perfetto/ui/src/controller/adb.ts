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

import {_TextDecoder, _TextEncoder} from 'custom_utils';

import {Adb, AdbMsg, AdbStream, CmdType} from './adb_interfaces';

const textEncoder = new _TextEncoder();
const textDecoder = new _TextDecoder();

export const VERSION_WITH_CHECKSUM = 0x01000000;
export const VERSION_NO_CHECKSUM = 0x01000001;
export const DEFAULT_MAX_PAYLOAD_BYTES = 256 * 1024;

export enum AdbState {
  DISCONNECTED = 0,
  // Authentication steps, see AdbOverWebUsb's handleAuthentication().
  AUTH_STEP1 = 1,
  AUTH_STEP2 = 2,
  AUTH_STEP3 = 3,

  CONNECTED = 2,
}

enum AuthCmd {
  TOKEN = 1,
  SIGNATURE = 2,
  RSAPUBLICKEY = 3,
}

const DEVICE_NOT_SET_ERROR = 'Device not set.';

// This class is a basic TypeScript implementation of adb that only supports
// shell commands. It is used to send the start tracing command to the connected
// android device, and to automatically pull the trace after the end of the
// recording. It works through the webUSB API. A brief description of how it
// works is the following:
// - The connection with the device is initiated by findAndConnect, which shows
//   a dialog with a list of connected devices. Once one is selected the
//   authentication begins. The authentication has to pass different steps, as
//   described in the "handeAuthentication" method.
// - AdbOverWebUsb tracks the state of the authentication via a state machine
//   (see AdbState).
// - A Message handler loop is executed to keep receiving the messages.
// - All the messages received from the device are passed to "onMessage" that is
//   implemented as a state machine.
// - When a new shell is established, it becomes an AdbStream, and is kept in
//   the "streams" map. Each time a message from the device is for a specific
//   previously opened stream, the "onMessage" function will forward it to the
//   stream (identified by a number).
export class AdbOverWebUsb implements Adb {
  state: AdbState = AdbState.DISCONNECTED;
  streams = new Map<number, AdbStream>();
  devProps = '';
  maxPayload = DEFAULT_MAX_PAYLOAD_BYTES;
  key?: CryptoKeyPair;
  onConnected = () => {};

  // Devices after Dec 2017 don't use checksum. This will be auto-detected
  // during the connection.
  useChecksum = true;

  private lastStreamId = 0;
  private dev: USBDevice|undefined;
  private usbReadEndpoint = -1;
  private usbWriteEpEndpoint = -1;
  private filter = {
    classCode: 255,    // USB vendor specific code
    subclassCode: 66,  // Android vendor specific subclass
    protocolCode: 1    // Adb protocol
  };

  async findDevice() {
    if (!('usb' in navigator)) {
      throw new Error('WebUSB not supported by the browser (requires HTTPS)');
    }
    return navigator.usb.requestDevice({filters: [this.filter]});
  }

  async getPairedDevices() {
    try {
      return navigator.usb.getDevices();
    } catch (e) {  // WebUSB not available.
      return Promise.resolve([]);
    }
  }

  async connect(device: USBDevice): Promise<void> {
    // If we are already connected, we are also already authenticated, so we can
    // skip doing the authentication again.
    if (this.state === AdbState.CONNECTED) {
      if (this.dev === device && device.opened) {
        this.onConnected();
        this.onConnected = () => {};
        return;
      }
      // Another device was connected.
      await this.disconnect();
    }

    this.dev = device;
    this.useChecksum = true;
    this.key = await AdbOverWebUsb.initKey();

    await this.dev.open();
    await this.dev.reset();  // The reset is done so that we can claim the
                             // device before adb server can.

    const {configValue, usbInterfaceNumber, endpoints} =
        this.findInterfaceAndEndpoint();

    this.usbReadEndpoint = this.findEndpointNumber(endpoints, 'in');
    this.usbWriteEpEndpoint = this.findEndpointNumber(endpoints, 'out');

    console.assert(this.usbReadEndpoint >= 0 && this.usbWriteEpEndpoint >= 0);

    await this.dev.selectConfiguration(configValue);
    await this.dev.claimInterface(usbInterfaceNumber);

    await this.startAuthentication();

    // This will start a message handler loop.
    this.receiveDeviceMessages();
    // The promise will be resolved after the handshake.
    return new Promise<void>((resolve, _) => this.onConnected = resolve);
  }

  async disconnect(): Promise<void> {
    this.state = AdbState.DISCONNECTED;

    if (!this.dev) return;

    new Map(this.streams).forEach((stream, _id) => stream.setClosed());
    console.assert(this.streams.size === 0);
    this.dev = undefined;
  }

  async startAuthentication() {
    // USB connected, now let's authenticate.
    const VERSION =
        this.useChecksum ? VERSION_WITH_CHECKSUM : VERSION_NO_CHECKSUM;
    this.state = AdbState.AUTH_STEP1;
    await this.send('CNXN', VERSION, this.maxPayload, 'host:1:UsbADB');
  }

  findInterfaceAndEndpoint() {
    if (!this.dev) throw Error(DEVICE_NOT_SET_ERROR);
    for (const config of this.dev.configurations) {
      for (const interface_ of config.interfaces) {
        for (const alt of interface_.alternates) {
          if (alt.interfaceClass === this.filter.classCode &&
              alt.interfaceSubclass === this.filter.subclassCode &&
              alt.interfaceProtocol === this.filter.protocolCode) {
            return {
              configValue: config.configurationValue,
              usbInterfaceNumber: interface_.interfaceNumber,
              endpoints: alt.endpoints
            };
          }  // if (alternate)
        }    // for (interface.alternates)
      }      // for (configuration.interfaces)
    }        // for (configurations)

    throw Error('Cannot find interfaces and endpoints');
  }

  findEndpointNumber(
      endpoints: USBEndpoint[], direction: 'out'|'in', type = 'bulk'): number {
    const ep =
        endpoints.find((ep) => ep.type === type && ep.direction === direction);

    if (ep) return ep.endpointNumber;

    throw Error(`Cannot find ${direction} endpoint`);
  }

  receiveDeviceMessages() {
    this.recv()
        .then(msg => {
          this.onMessage(msg);
          this.receiveDeviceMessages();
        })
        .catch(e => {
          // Ignore error with "DEVICE_NOT_SET_ERROR" message since it is always
          // thrown after the device disconnects.
          if (e.message !== DEVICE_NOT_SET_ERROR) {
            console.error(`Exception in recv: ${e.name}. error: ${e.message}`);
          }
          this.disconnect();
        });
  }

  async onMessage(msg: AdbMsg) {
    if (!this.key) throw Error('ADB key not initialized');

    if (msg.cmd === 'AUTH' && msg.arg0 === AuthCmd.TOKEN) {
      this.handleAuthentication(msg);
    } else if (msg.cmd === 'CNXN') {
      console.assert(
          [AdbState.AUTH_STEP2, AdbState.AUTH_STEP3].includes(this.state));
      this.state = AdbState.CONNECTED;
      this.handleConnectedMessage(msg);
    } else if (this.state === AdbState.CONNECTED && [
                 'OKAY',
                 'WRTE',
                 'CLSE'
               ].indexOf(msg.cmd) >= 0) {
      const stream = this.streams.get(msg.arg1);
      if (!stream) {
        console.warn(`Received message ${msg} for unknown stream ${msg.arg1}`);
        return;
      }
      stream.onMessage(msg);
    } else {
      console.error(`Unexpected message `, msg, ` in state ${this.state}`);
    }
  }

  async handleAuthentication(msg: AdbMsg) {
    if (!this.key) throw Error('ADB key not initialized');

    console.assert(msg.cmd === 'AUTH' && msg.arg0 === AuthCmd.TOKEN);
    const token = msg.data;

    if (this.state === AdbState.AUTH_STEP1) {
      // During this step, we send back the token received signed with our
      // private key. If the device has previously received our public key, the
      // dialog will not be displayed. Otherwise we will receive another message
      // ending up in AUTH_STEP3.
      this.state = AdbState.AUTH_STEP2;

      const signedToken =
          await signAdbTokenWithPrivateKey(this.key.privateKey, token);
      this.send('AUTH', AuthCmd.SIGNATURE, 0, new Uint8Array(signedToken));
      return;
    }

    console.assert(this.state === AdbState.AUTH_STEP2);

    // During this step, we send our public key. The dialog will appear, and
    // if the user chooses to remember our public key, it will be
    // saved, so that the next time we will only pass through AUTH_STEP1.
    this.state = AdbState.AUTH_STEP3;
    const encodedPubKey = await encodePubKey(this.key.publicKey);
    this.send('AUTH', AuthCmd.RSAPUBLICKEY, 0, encodedPubKey);
  }

  private handleConnectedMessage(msg: AdbMsg) {
    console.assert(msg.cmd === 'CNXN');

    this.maxPayload = msg.arg1;
    this.devProps = textDecoder.decode(msg.data);

    const deviceVersion = msg.arg0;

    if (![VERSION_WITH_CHECKSUM, VERSION_NO_CHECKSUM].includes(deviceVersion)) {
      console.error('Version ', msg.arg0, ' not really supported!');
    }
    this.useChecksum = deviceVersion === VERSION_WITH_CHECKSUM;
    this.state = AdbState.CONNECTED;

    // This will resolve the promise returned by "onConnect"
    this.onConnected();
    this.onConnected = () => {};
  }

  shell(cmd: string): Promise<AdbStream> {
    return this.openStream('shell:' + cmd);
  }

  socket(path: string): Promise<AdbStream> {
    return this.openStream('localfilesystem:' + path);
  }

  openStream(svc: string): Promise<AdbStream> {
    const stream = new AdbStreamImpl(this, ++this.lastStreamId);
    this.streams.set(stream.localStreamId, stream);
    this.send('OPEN', stream.localStreamId, 0, svc);

    //  The stream will resolve this promise once it receives the
    //  acknowledgement message from the device.
    return new Promise<AdbStream>((resolve, reject) => {
      stream.onConnect = () => {
        stream.onClose = () => {};
        resolve(stream);
      };
      stream.onClose = () => reject();
    });
  }

  async shellOutputAsString(cmd: string): Promise<string> {
    const shell = await this.shell(cmd);

    return new Promise<string>((resolve, _) => {
      const output: string[] = [];
      shell.onData = raw => output.push(textDecoder.decode(raw));
      shell.onClose = () => resolve(output.join());
    });
  }

  async send(
      cmd: CmdType, arg0: number, arg1: number, data?: Uint8Array|string) {
    await this.sendMsg(AdbMsgImpl.create(
        {cmd, arg0, arg1, data, useChecksum: this.useChecksum}));
  }

  //  The header and the message data must be sent consecutively. Using 2 awaits
  //  Another message can interleave after the first header has been sent,
  //  resulting in something like [header1] [header2] [data1] [data2];
  //  In this way we are waiting both promises to be resolved before continuing.
  async sendMsg(msg: AdbMsgImpl) {
    const sendPromises = [this.sendRaw(msg.encodeHeader())];
    if (msg.data.length > 0) sendPromises.push(this.sendRaw(msg.data));
    await Promise.all(sendPromises);
  }

  async recv(): Promise<AdbMsg> {
    const res = await this.recvRaw(ADB_MSG_SIZE);
    console.assert(res.status === 'ok');
    const msg = AdbMsgImpl.decodeHeader(res.data!);

    if (msg.dataLen > 0) {
      const resp = await this.recvRaw(msg.dataLen);
      msg.data = new Uint8Array(
          resp.data!.buffer, resp.data!.byteOffset, resp.data!.byteLength);
    }
    if (this.useChecksum) {
      console.assert(AdbOverWebUsb.checksum(msg.data) === msg.dataChecksum);
    }
    return msg;
  }

  static async initKey(): Promise<CryptoKeyPair> {
    const KEY_SIZE = 2048;

    const keySpec = {
      name: 'RSASSA-PKCS1-v1_5',
      modulusLength: KEY_SIZE,
      publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
      hash: {name: 'SHA-1'},
    };

    const key = await crypto.subtle.generateKey(
        keySpec, /*extractable=*/ true, ['sign', 'verify']);

    return key;
  }

  static checksum(data: Uint8Array): number {
    let res = 0;
    for (let i = 0; i < data.byteLength; i++) res += data[i];
    return res & 0xFFFFFFFF;
  }

  sendRaw(buf: Uint8Array): Promise<USBOutTransferResult> {
    console.assert(buf.length <= this.maxPayload);
    if (!this.dev) throw Error(DEVICE_NOT_SET_ERROR);
    return this.dev.transferOut(this.usbWriteEpEndpoint, buf.buffer);
  }

  recvRaw(dataLen: number): Promise<USBInTransferResult> {
    if (!this.dev) throw Error(DEVICE_NOT_SET_ERROR);
    return this.dev.transferIn(this.usbReadEndpoint, dataLen);
  }
}

enum AdbStreamState {
  WAITING_INITIAL_OKAY = 0,
  CONNECTED = 1,
  CLOSED = 2
}


// An AdbStream is instantiated after the creation of a shell to the device.
// Thanks to this, we can send commands and receive their output. Messages are
// received in the main adb class, and are forwarded to an instance of this
// class based on a stream id match. Also streams have an initialization flow:
//   1. WAITING_INITIAL_OKAY: waiting for first "OKAY" message. Once received,
//      the next state will be "CONNECTED".
//   2. CONNECTED: ready to receive or send messages.
//   3. WRITING: this is needed because we must receive an ack after sending
//      each message (so, before sending the next one). For this reason, many
//      subsequent "write" calls will result in different messages in the
//      writeQueue. After each new acknowledgement ('OKAY') a new one will be
//      sent. When the queue is empty, the state will return to CONNECTED.
//   4. CLOSED: entered when the device closes the stream or close() is called.
//      For shell commands, the stream is closed after the command completed.
export class AdbStreamImpl implements AdbStream {
  private adb: AdbOverWebUsb;
  localStreamId: number;
  private remoteStreamId = -1;
  private state: AdbStreamState = AdbStreamState.WAITING_INITIAL_OKAY;
  private writeQueue: Uint8Array[] = [];

  private sendInProgress = false;

  onData: AdbStreamReadCallback = (_) => {};
  onConnect = () => {};
  onClose = () => {};

  constructor(adb: AdbOverWebUsb, localStreamId: number) {
    this.adb = adb;
    this.localStreamId = localStreamId;
  }

  close() {
    console.assert(this.state === AdbStreamState.CONNECTED);

    if (this.writeQueue.length > 0) {
      console.error(`Dropping ${
          this.writeQueue.length} queued messages due to stream closing.`);
      this.writeQueue = [];
    }

    this.adb.send('CLSE', this.localStreamId, this.remoteStreamId);
  }

  async write(msg: string|Uint8Array) {
    const raw = (typeof msg === 'string') ? textEncoder.encode(msg) : msg;
    if (this.sendInProgress ||
        this.state === AdbStreamState.WAITING_INITIAL_OKAY) {
      this.writeQueue.push(raw);
      return;
    }
    console.assert(this.state === AdbStreamState.CONNECTED);
    this.sendInProgress = true;
    await this.adb.send('WRTE', this.localStreamId, this.remoteStreamId, raw);
  }

  setClosed() {
    this.state = AdbStreamState.CLOSED;
    this.adb.streams.delete(this.localStreamId);
    this.onClose();
  }

  onMessage(msg: AdbMsgImpl) {
    console.assert(msg.arg1 === this.localStreamId);

    if (this.state === AdbStreamState.WAITING_INITIAL_OKAY &&
        msg.cmd === 'OKAY') {
      this.remoteStreamId = msg.arg0;
      this.state = AdbStreamState.CONNECTED;
      this.onConnect();
      return;
    }

    if (msg.cmd === 'WRTE') {
      this.adb.send('OKAY', this.localStreamId, this.remoteStreamId);
      this.onData(msg.data);
      return;
    }

    if (msg.cmd === 'OKAY') {
      console.assert(this.sendInProgress);
      this.sendInProgress = false;
      const queuedMsg = this.writeQueue.shift();
      if (queuedMsg !== undefined) this.write(queuedMsg);
      return;
    }

    if (msg.cmd === 'CLSE') {
      this.setClosed();
      return;
    }
    console.error(
        `Unexpected stream msg ${msg.toString()} in state ${this.state}`);
  }
}

interface AdbStreamReadCallback {
  (raw: Uint8Array): void;
}

const ADB_MSG_SIZE = 6 * 4;  // 6 * int32.

export class AdbMsgImpl implements AdbMsg {
  cmd: CmdType;
  arg0: number;
  arg1: number;
  data: Uint8Array;
  dataLen: number;
  dataChecksum: number;

  useChecksum: boolean;

  constructor(
      cmd: CmdType, arg0: number, arg1: number, dataLen: number,
      dataChecksum: number, useChecksum = false) {
    console.assert(cmd.length === 4);
    this.cmd = cmd;
    this.arg0 = arg0;
    this.arg1 = arg1;
    this.dataLen = dataLen;
    this.data = new Uint8Array(dataLen);
    this.dataChecksum = dataChecksum;
    this.useChecksum = useChecksum;
  }


  static create({cmd, arg0, arg1, data, useChecksum = true}: {
    cmd: CmdType; arg0: number; arg1: number;
    data?: Uint8Array | string;
    useChecksum?: boolean;
  }): AdbMsgImpl {
    const encodedData = this.encodeData(data);
    const msg =
        new AdbMsgImpl(cmd, arg0, arg1, encodedData.length, 0, useChecksum);
    msg.data = encodedData;
    return msg;
  }

  get dataStr() {
    return textDecoder.decode(this.data);
  }

  toString() {
    return `${this.cmd} [${this.arg0},${this.arg1}] ${this.dataStr}`;
  }

  // A brief description of the message can be found here:
  // https://android.googlesource.com/platform/system/core/+/master/adb/protocol.txt
  //
  // struct amessage {
  //     uint32_t command;    // command identifier constant
  //     uint32_t arg0;       // first argument
  //     uint32_t arg1;       // second argument
  //     uint32_t data_length;// length of payload (0 is allowed)
  //     uint32_t data_check; // checksum of data payload
  //     uint32_t magic;      // command ^ 0xffffffff
  // };
  static decodeHeader(dv: DataView): AdbMsgImpl {
    console.assert(dv.byteLength === ADB_MSG_SIZE);
    const cmd = textDecoder.decode(dv.buffer.slice(0, 4)) as CmdType;
    const cmdNum = dv.getUint32(0, true);
    const arg0 = dv.getUint32(4, true);
    const arg1 = dv.getUint32(8, true);
    const dataLen = dv.getUint32(12, true);
    const dataChecksum = dv.getUint32(16, true);
    const cmdChecksum = dv.getUint32(20, true);
    console.assert(cmdNum === (cmdChecksum ^ 0xFFFFFFFF));
    return new AdbMsgImpl(cmd, arg0, arg1, dataLen, dataChecksum);
  }

  encodeHeader(): Uint8Array {
    const buf = new Uint8Array(ADB_MSG_SIZE);
    const dv = new DataView(buf.buffer);
    const cmdBytes: Uint8Array = textEncoder.encode(this.cmd);
    const rawMsg = AdbMsgImpl.encodeData(this.data);
    const checksum = this.useChecksum ? AdbOverWebUsb.checksum(rawMsg) : 0;
    for (let i = 0; i < 4; i++) dv.setUint8(i, cmdBytes[i]);

    dv.setUint32(4, this.arg0, true);
    dv.setUint32(8, this.arg1, true);
    dv.setUint32(12, rawMsg.byteLength, true);
    dv.setUint32(16, checksum, true);
    dv.setUint32(20, dv.getUint32(0, true) ^ 0xFFFFFFFF, true);

    return buf;
  }

  static encodeData(data?: Uint8Array|string): Uint8Array {
    if (data === undefined) return new Uint8Array([]);
    if (typeof data === 'string') return textEncoder.encode(data + '\0');
    return data;
  }
}


function base64StringToArray(s: string) {
  const decoded = atob(s.replace(/-/g, '+').replace(/_/g, '/'));
  return [...decoded].map(char => char.charCodeAt(0));
}

const ANDROID_PUBKEY_MODULUS_SIZE = 2048;
const MODULUS_SIZE_BYTES = ANDROID_PUBKEY_MODULUS_SIZE / 8;

// RSA Public keys are encoded in a rather unique way. It's a base64 encoded
// struct of 524 bytes in total as follows (see
// libcrypto_utils/android_pubkey.c):
//
// typedef struct RSAPublicKey {
//   // Modulus length. This must be ANDROID_PUBKEY_MODULUS_SIZE.
//   uint32_t modulus_size_words;
//
//   // Precomputed montgomery parameter: -1 / n[0] mod 2^32
//   uint32_t n0inv;
//
//   // RSA modulus as a little-endian array.
//   uint8_t modulus[ANDROID_PUBKEY_MODULUS_SIZE];
//
//   // Montgomery parameter R^2 as a little-endian array of little-endian
//   words. uint8_t rr[ANDROID_PUBKEY_MODULUS_SIZE];
//
//   // RSA modulus: 3 or 65537
//   uint32_t exponent;
// } RSAPublicKey;
//
// However, the Montgomery params (n0inv and rr) are not really used, see
// comment in android_pubkey_decode() ("Note that we don't extract the
// montgomery parameters...")
async function encodePubKey(key: CryptoKey) {
  const expPubKey = await crypto.subtle.exportKey('jwk', key);
  const nArr = base64StringToArray(expPubKey.n as string).reverse();
  const eArr = base64StringToArray(expPubKey.e as string).reverse();

  const arr = new Uint8Array(3 * 4 + 2 * MODULUS_SIZE_BYTES);
  const dv = new DataView(arr.buffer);
  dv.setUint32(0, MODULUS_SIZE_BYTES / 4, true);

  // The Mongomery params (n0inv and rr) are not computed.
  dv.setUint32(4, 0 /*n0inv*/, true);
  // Modulus
  for (let i = 0; i < MODULUS_SIZE_BYTES; i++) dv.setUint8(8 + i, nArr[i]);

  // rr:
  for (let i = 0; i < MODULUS_SIZE_BYTES; i++) {
    dv.setUint8(8 + MODULUS_SIZE_BYTES + i, 0 /*rr*/);
  }
  // Exponent
  for (let i = 0; i < 4; i++) {
    dv.setUint8(8 + (2 * MODULUS_SIZE_BYTES) + i, eArr[i]);
  }
  return btoa(String.fromCharCode(...new Uint8Array(dv.buffer))) +
      ' perfetto@webusb';
}

// TODO(nicomazz): This token signature will be useful only when we save the
// generated keys. So far, we are not doing so. As a consequence, a dialog is
// displayed every time a tracing session is started.
// The reason why it has not already been implemented is that the standard
// crypto.subtle.sign function assumes that the input needs hashing, which is
// not the case for ADB, where the 20 bytes token is already hashed.
// A solution to this is implementing a custom private key signature with a js
// implementation of big integers. Maybe, wrapping the key like in the following
// CL can work:
// https://android-review.googlesource.com/c/platform/external/perfetto/+/1105354/18
async function signAdbTokenWithPrivateKey(
    _privateKey: CryptoKey, token: Uint8Array): Promise<ArrayBuffer> {
  // This function is not implemented.
  return token.buffer;
}
