'use strict'

// Convert `manufacturerData` to an array of bluetooth.BluetoothManufacturerData
// defined in
// https://webbluetoothcg.github.io/web-bluetooth/#bluetooth-bidi-definitions.
function convertToBidiManufacturerData(manufacturerData) {
  const bidiManufacturerData = [];
  for (const key in manufacturerData) {
    bidiManufacturerData.push({
      key: parseInt(key),
      data: btoa(String.fromCharCode(...manufacturerData[key]))
    })
  }
  return bidiManufacturerData;
}

function ArrayToMojoCharacteristicProperties(arr) {
  const struct = {};
  arr.forEach(property => {
    struct[property] = true;
  });
  return struct;
}

class FakeBluetooth {
  constructor() {
    this.fake_central_ = null;
  }

  // Returns a promise that resolves with a FakeCentral that clients can use
  // to simulate events that a device in the Central/Observer role would
  // receive as well as monitor the operations performed by the device in the
  // Central/Observer role.
  //
  // A "Central" object would allow its clients to receive advertising events
  // and initiate connections to peripherals i.e. operations of two roles
  // defined by the Bluetooth Spec: Observer and Central.
  // See Bluetooth 4.2 Vol 3 Part C 2.2.2 "Roles when Operating over an
  // LE Physical Transport".
  async simulateCentral({state}) {
    if (this.fake_central_) {
      throw 'simulateCentral() should only be called once';
    }

    await test_driver.bidi.bluetooth.simulate_adapter({state: state});
    this.fake_central_ = new FakeCentral();
    return this.fake_central_;
  }
}

// FakeCentral allows clients to simulate events that a device in the
// Central/Observer role would receive as well as monitor the operations
// performed by the device in the Central/Observer role.
class FakeCentral {
  constructor() {
    this.peripherals_ = new Map();
  }

  // Simulates a peripheral with |address|, |name|, |manufacturerData| and
  // |known_service_uuids| that has already been connected to the system. If the
  // peripheral existed already it updates its name, manufacturer data, and
  // known UUIDs. |known_service_uuids| should be an array of
  // BluetoothServiceUUIDs
  // https://webbluetoothcg.github.io/web-bluetooth/#typedefdef-bluetoothserviceuuid
  //
  // Platforms offer methods to retrieve devices that have already been
  // connected to the system or weren't connected through the UA e.g. a user
  // connected a peripheral through the system's settings. This method is
  // intended to simulate peripherals that those methods would return.
  async simulatePreconnectedPeripheral(
      {address, name, manufacturerData = {}, knownServiceUUIDs = []}) {
    await test_driver.bidi.bluetooth.simulate_preconnected_peripheral({
      address: address,
      name: name,
      manufacturerData: convertToBidiManufacturerData(manufacturerData),
      knownServiceUuids:
          knownServiceUUIDs.map(uuid => BluetoothUUID.getService(uuid))
    });

    return this.fetchOrCreatePeripheral_(address);
  }

  // Create a fake_peripheral object from the given address.
  fetchOrCreatePeripheral_(address) {
    let peripheral = this.peripherals_.get(address);
    if (peripheral === undefined) {
      peripheral = new FakePeripheral(address);
      this.peripherals_.set(address, peripheral);
    }
    return peripheral;
  }
}

class FakePeripheral {
  constructor(address) {
    this.address = address;
  }

  // Adds a fake GATT Service with |uuid| to be discovered when discovering
  // the peripheral's GATT Attributes. Returns a FakeRemoteGATTService
  // corresponding to this service. |uuid| should be a BluetoothServiceUUIDs
  // https://webbluetoothcg.github.io/web-bluetooth/#typedefdef-bluetoothserviceuuid
  async addFakeService({uuid}) {
    const service_uuid = BluetoothUUID.getService(uuid);
    await test_driver.bidi.bluetooth.simulate_service({
      address: this.address,
      uuid: service_uuid,
      type: 'add',
    });
    return new FakeRemoteGATTService(service_uuid, this.address);
  }

  // Sets the next GATT Connection request response to |code|. |code| could be
  // an HCI Error Code from BT 4.2 Vol 2 Part D 1.3 List Of Error Codes or a
  // number outside that range returned by specific platforms e.g. Android
  // returns 0x101 to signal a GATT failure
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#GATT_FAILURE
  async setNextGATTConnectionResponse({code}) {
    const remove_handler =
        test_driver.bidi.bluetooth.gatt_connection_attempted.on((event) => {
          if (event.address != this.address) {
            return;
          }
          remove_handler();
          test_driver.bidi.bluetooth.simulate_gatt_connection_response({
            address: event.address,
            code,
          });
        });
  }

  async setNextGATTDiscoveryResponse({code}) {
    // No-op for Web Bluetooth Bidi test, it will be removed when migration
    // completes.
    return Promise.resolve();
  }

  // Simulates a GATT connection response with |code| from the peripheral.
  async simulateGATTConnectionResponse(code) {
    await test_driver.bidi.bluetooth.simulate_gatt_connection_response(
        {address: this.address, code});
  }

  // Simulates a GATT disconnection in the peripheral.
  async simulateGATTDisconnection() {
    await test_driver.bidi.bluetooth.simulate_gatt_disconnection(
        {address: this.address});
  }
}

class FakeRemoteGATTService {
  constructor(service_uuid, peripheral_address) {
    this.service_uuid_ = service_uuid;
    this.peripheral_address_ = peripheral_address;
  }

  // Adds a fake GATT Characteristic with |uuid| and |properties|
  // to this fake service. The characteristic will be found when discovering
  // the peripheral's GATT Attributes. Returns a FakeRemoteGATTCharacteristic
  // corresponding to the added characteristic.
  async addFakeCharacteristic({uuid, properties}) {
    const characteristic_uuid = BluetoothUUID.getCharacteristic(uuid);
    await test_driver.bidi.bluetooth.simulate_characteristic({
      address: this.peripheral_address_,
      serviceUuid: this.service_uuid_,
      characteristicUuid: characteristic_uuid,
      characteristicProperties: ArrayToMojoCharacteristicProperties(properties),
      type: 'add'
    });
    return new FakeRemoteGATTCharacteristic(
        characteristic_uuid, this.service_uuid_, this.peripheral_address_);
  }

  // Removes the fake GATT service from its fake peripheral.
  async remove() {
    await test_driver.bidi.bluetooth.simulate_service({
      address: this.peripheral_address_,
      uuid: this.service_uuid_,
      type: 'remove'
    });
  }
}

class FakeRemoteGATTCharacteristic {
  constructor(characteristic_uuid, service_uuid, peripheral_address) {
    this.characteristic_uuid_ = characteristic_uuid;
    this.service_uuid_ = service_uuid;
    this.peripheral_address_ = peripheral_address;
    this.last_written_value_ = {lastValue: null, lastWriteType: 'none'};
  }

  // Adds a fake GATT Descriptor with |uuid| to be discovered when
  // discovering the peripheral's GATT Attributes. Returns a
  // FakeRemoteGATTDescriptor corresponding to this descriptor. |uuid| should
  // be a BluetoothDescriptorUUID
  // https://webbluetoothcg.github.io/web-bluetooth/#typedefdef-bluetoothdescriptoruuid
  async addFakeDescriptor({uuid}) {
    const descriptor_uuid = BluetoothUUID.getDescriptor(uuid);
    await test_driver.bidi.bluetooth.simulate_descriptor({
      address: this.peripheral_address_,
      serviceUuid: this.service_uuid_,
      characteristicUuid: this.characteristic_uuid_,
      descriptorUuid: descriptor_uuid,
      type: 'add'
    });
    return new FakeRemoteGATTDescriptor(
        descriptor_uuid, this.characteristic_uuid_, this.service_uuid_,
        this.peripheral_address_);
  }

  // Simulate a characteristic for operation |type| with response |code| and
  // |data|.
  async simulateResponse(type, code, data) {
    await test_driver.bidi.bluetooth.simulate_characteristic_response({
      address: this.peripheral_address_,
      serviceUuid: this.service_uuid_,
      characteristicUuid: this.characteristic_uuid_,
      type,
      code,
      data,
    });
  }

  // Simulate a characteristic response for read operation with response |code|
  // and |data|.
  async simulateReadResponse(code, data) {
    await this.simulateResponse('read', code, data);
  }

  // Simulate a characteristic response for write operation with response
  // |code|.
  async simulateWriteResponse(code) {
    await this.simulateResponse('write', code);
  }

  // Sets the next read response for characteristic to |code| and |value|.
  // |code| could be a GATT Error Response from
  // BT 4.2 Vol 3 Part F 3.4.1.1 Error Response or a number outside that range
  // returned by specific platforms e.g. Android returns 0x101 to signal a GATT
  // failure.
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#GATT_FAILURE
  async setNextReadResponse(gatt_code, value = null) {
    if (gatt_code === 0 && value === null) {
      throw '|value| can\'t be null if read should success.';
    }
    if (gatt_code !== 0 && value !== null) {
      throw '|value| must be null if read should fail.';
    }

    const remove_handler =
        test_driver.bidi.bluetooth.characteristic_event_generated.on(
            (event) => {
              if (event.address != this.peripheral_address_) {
                return;
              }
              remove_handler();
              this.simulateReadResponse(gatt_code, value);
            });
  }

  // Sets the next write response for this characteristic to |code|. If
  // writing to a characteristic that only supports 'write-without-response'
  // the set response will be ignored.
  // |code| could be a GATT Error Response from
  // BT 4.2 Vol 3 Part F 3.4.1.1 Error Response or a number outside that range
  // returned by specific platforms e.g. Android returns 0x101 to signal a GATT
  // failure.
  async setNextWriteResponse(gatt_code) {
    const remove_handler =
        test_driver.bidi.bluetooth.characteristic_event_generated.on(
            (event) => {
              if (event.address != this.peripheral_address_) {
                return;
              }
              this.last_written_value_ = {
                lastValue: event.data,
                lastWriteType: event.type
              };
              remove_handler();
              if (event.type == 'write-with-response') {
                this.simulateWriteResponse(gatt_code);
              }
            });
  }

  // Gets the last successfully written value to the characteristic and its
  // write type. Write type is one of 'none', 'default-deprecated',
  // 'with-response', 'without-response'. Returns {lastValue: null,
  // lastWriteType: 'none'} if no value has yet been written to the
  // characteristic.
  async getLastWrittenValue() {
    return this.last_written_value_;
  }

  // Removes the fake GATT Characteristic from its fake service.
  async remove() {
    await test_driver.bidi.bluetooth.simulate_characteristic({
      address: this.peripheral_address_,
      serviceUuid: this.service_uuid_,
      characteristicUuid: this.characteristic_uuid_,
      characteristicProperties: undefined,
      type: 'remove'
    });
  }
}

class FakeRemoteGATTDescriptor {
  constructor(
      descriptor_uuid, characteristic_uuid, service_uuid, peripheral_address) {
    this.descriptor_uuid_ = descriptor_uuid;
    this.characteristic_uuid_ = characteristic_uuid;
    this.service_uuid_ = service_uuid;
    this.peripheral_address_ = peripheral_address;
    this.last_written_value_ = null;
  }

  // Simulate a descriptor for operation |type| with response |code| and
  // |data|.
  async simulateResponse(type, code, data) {
    await test_driver.bidi.bluetooth.simulate_descriptor_response({
      address: this.peripheral_address_,
      serviceUuid: this.service_uuid_,
      characteristicUuid: this.characteristic_uuid_,
      descriptorUuid: this.descriptor_uuid_,
      type,
      code,
      data,
    });
  }

  // Simulate a descriptor response for read operation with response |code| and
  // |data|.
  async simulateReadResponse(code, data) {
    await this.simulateResponse('read', code, data);
  }

  // Simulate a descriptor response for write operation with response |code|.
  async simulateWriteResponse(code) {
    await this.simulateResponse('write', code);
  }

  // Sets the next read response for descriptor to |code| and |value|.
  // |code| could be a GATT Error Response from
  // BT 4.2 Vol 3 Part F 3.4.1.1 Error Response or a number outside that range
  // returned by specific platforms e.g. Android returns 0x101 to signal a GATT
  // failure.
  // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#GATT_FAILURE
  async setNextReadResponse(gatt_code, value = null) {
    if (gatt_code === 0 && value === null) {
      throw '|value| can\'t be null if read should success.';
    }
    if (gatt_code !== 0 && value !== null) {
      throw '|value| must be null if read should fail.';
    }

    const remove_handler =
        test_driver.bidi.bluetooth.descriptor_event_generated.on((event) => {
          if (event.address != this.peripheral_address_) {
            return;
          }
          remove_handler();
          this.simulateReadResponse(gatt_code, value);
        });
  }

  // Sets the next write response for this descriptor to |code|.
  // |code| could be a GATT Error Response from
  // BT 4.2 Vol 3 Part F 3.4.1.1 Error Response or a number outside that range
  // returned by specific platforms e.g. Android returns 0x101 to signal a GATT
  // failure.
  async setNextWriteResponse(gatt_code) {
    const remove_handler =
        test_driver.bidi.bluetooth.descriptor_event_generated.on((event) => {
          if (event.address != this.peripheral_address_) {
            return;
          }
          this.last_written_value_ = {
            lastValue: event.data,
            lastWriteType: event.type
          };
          remove_handler();
          if (event.type == 'write-with-response') {
            this.simulateWriteResponse(gatt_code);
          }
        });
  }

  // Gets the last successfully written value to the descriptor.
  // Returns null if no value has yet been written to the descriptor.
  async getLastWrittenValue() {
    return this.last_written_value_;
  }

  // Removes the fake GATT Descriptor from its fake characteristic.
  async remove() {
    await test_driver.bidi.bluetooth.simulate_descriptor({
      address: this.peripheral_address_,
      serviceUuid: this.service_uuid_,
      characteristicUuid: this.characteristic_uuid_,
      descriptorUuid: this.descriptor_uuid_,
      type: 'remove'
    });
  }
}

function initializeBluetoothBidiResources() {
  navigator.bluetooth.test = new FakeBluetooth();
}
