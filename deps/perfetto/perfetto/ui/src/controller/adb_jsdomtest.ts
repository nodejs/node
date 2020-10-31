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

import {TextEncoder} from 'util';

import {
  AdbMsgImpl,
  AdbOverWebUsb,
  AdbState,
  DEFAULT_MAX_PAYLOAD_BYTES,
  VERSION_WITH_CHECKSUM
} from './adb';

test('startAuthentication', async () => {
  const adb = new AdbOverWebUsb();

  const sendRaw = jest.fn();
  adb.sendRaw = sendRaw;
  const recvRaw = jest.fn();
  adb.recvRaw = recvRaw;


  const expectedAuthMessage = AdbMsgImpl.create({
    cmd: 'CNXN',
    arg0: VERSION_WITH_CHECKSUM,
    arg1: DEFAULT_MAX_PAYLOAD_BYTES,
    data: 'host:1:UsbADB',
    useChecksum: true
  });
  await adb.startAuthentication();

  expect(sendRaw).toHaveBeenCalledTimes(2);
  expect(sendRaw).toBeCalledWith(expectedAuthMessage.encodeHeader());
  expect(sendRaw).toBeCalledWith(expectedAuthMessage.data);
});

test('connectedMessage', async () => {
  const adb = new AdbOverWebUsb();
  adb.key = {} as unknown as CryptoKeyPair;
  adb.state = AdbState.AUTH_STEP2;

  const onConnected = jest.fn();
  adb.onConnected = onConnected;

  const expectedMaxPayload = 42;
  const connectedMsg = AdbMsgImpl.create({
    cmd: 'CNXN',
    arg0: VERSION_WITH_CHECKSUM,
    arg1: expectedMaxPayload,
    data: new TextEncoder().encode('device'),
    useChecksum: true
  });
  await adb.onMessage(connectedMsg);

  expect(adb.state).toBe(AdbState.CONNECTED);
  expect(adb.maxPayload).toBe(expectedMaxPayload);
  expect(adb.devProps).toBe('device');
  expect(adb.useChecksum).toBe(true);
  expect(onConnected).toHaveBeenCalledTimes(1);
});


test('shellOpening', () => {
  const adb = new AdbOverWebUsb();
  const openStream = jest.fn();
  adb.openStream = openStream;

  adb.shell('test');
  expect(openStream).toBeCalledWith('shell:test');
});
