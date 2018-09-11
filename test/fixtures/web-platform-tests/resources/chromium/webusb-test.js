'use strict';

// This polyfill library implements the WebUSB Test API as specified here:
// https://wicg.github.io/webusb/test/

(() => {

// These variables are logically members of the USBTest class but are defined
// here to hide them from being visible as fields of navigator.usb.test.
let internal = {
  intialized: false,

  webUsbService: null,
  webUsbServiceInterceptor: null,
  webUsbServiceCrossFrameProxy: null,
};

// Converts an ECMAScript String object to an instance of
// mojo_base.mojom.String16.
function mojoString16ToString(string16) {
  return String.fromCharCode.apply(null, string16.data);
}

// Converts an instance of mojo_base.mojom.String16 to an ECMAScript String.
function stringToMojoString16(string) {
  let array = new Array(string.length);
  for (var i = 0; i < string.length; ++i) {
    array[i] = string.charCodeAt(i);
  }
  return { data: array }
}

function fakeDeviceInitToDeviceInfo(guid, init) {
  let deviceInfo = {
    guid: guid + "",
    usbVersionMajor: init.usbVersionMajor,
    usbVersionMinor: init.usbVersionMinor,
    usbVersionSubminor: init.usbVersionSubminor,
    classCode: init.deviceClass,
    subclassCode: init.deviceSubclass,
    protocolCode: init.deviceProtocol,
    vendorId: init.vendorId,
    productId: init.productId,
    deviceVersionMajor: init.deviceVersionMajor,
    deviceVersionMinor: init.deviceVersionMinor,
    deviceVersionSubminor: init.deviceVersionSubminor,
    manufacturerName: stringToMojoString16(init.manufacturerName),
    productName: stringToMojoString16(init.productName),
    serialNumber: stringToMojoString16(init.serialNumber),
    activeConfiguration: init.activeConfigurationValue,
    configurations: []
  };
  init.configurations.forEach(config => {
    var configInfo = {
      configurationValue: config.configurationValue,
      configurationName: stringToMojoString16(config.configurationName),
      interfaces: []
    };
    config.interfaces.forEach(iface => {
      var interfaceInfo = {
        interfaceNumber: iface.interfaceNumber,
        alternates: []
      };
      iface.alternates.forEach(alternate => {
        var alternateInfo = {
          alternateSetting: alternate.alternateSetting,
          classCode: alternate.interfaceClass,
          subclassCode: alternate.interfaceSubclass,
          protocolCode: alternate.interfaceProtocol,
          interfaceName: stringToMojoString16(alternate.interfaceName),
          endpoints: []
        };
        alternate.endpoints.forEach(endpoint => {
          var endpointInfo = {
            endpointNumber: endpoint.endpointNumber,
            packetSize: endpoint.packetSize,
          };
          switch (endpoint.direction) {
          case "in":
            endpointInfo.direction = device.mojom.UsbTransferDirection.INBOUND;
            break;
          case "out":
            endpointInfo.direction = device.mojom.UsbTransferDirection.OUTBOUND;
            break;
          }
          switch (endpoint.type) {
          case "bulk":
            endpointInfo.type = device.mojom.UsbTransferType.BULK;
            break;
          case "interrupt":
            endpointInfo.type = device.mojom.UsbTransferType.INTERRUPT;
            break;
          case "isochronous":
            endpointInfo.type = device.mojom.UsbTransferType.ISOCHRONOUS;
            break;
          }
          alternateInfo.endpoints.push(endpointInfo);
        });
        interfaceInfo.alternates.push(alternateInfo);
      });
      configInfo.interfaces.push(interfaceInfo);
    });
    deviceInfo.configurations.push(configInfo);
  });
  return deviceInfo;
}

function convertMojoDeviceFilters(input) {
  let output = [];
  input.forEach(filter => {
    output.push(convertMojoDeviceFilter(filter));
  });
  return output;
}

function convertMojoDeviceFilter(input) {
  let output = {};
  if (input.hasVendorId)
    output.vendorId = input.vendorId;
  if (input.hasProductId)
    output.productId = input.productId;
  if (input.hasClassCode)
    output.classCode = input.classCode;
  if (input.hasSubclassCode)
    output.subclassCode = input.subclassCode;
  if (input.hasProtocolCode)
    output.protocolCode = input.protocolCode;
  if (input.serialNumber)
    output.serialNumber = mojoString16ToString(input.serialNumber);
  return output;
}

class FakeDevice {
  constructor(deviceInit) {
    this.info_ = deviceInit;
    this.opened_ = false;
    this.currentConfiguration_ = null;
    this.claimedInterfaces_ = new Map();
  }

  getConfiguration() {
    if (this.currentConfiguration_) {
      return Promise.resolve({
          value: this.currentConfiguration_.configurationValue });
    } else {
      return Promise.resolve({ value: 0 });
    }
  }

  open() {
    assert_false(this.opened_);
    this.opened_ = true;
    return Promise.resolve({ error: device.mojom.UsbOpenDeviceError.OK });
  }

  close() {
    assert_true(this.opened_);
    this.opened_ = false;
    return Promise.resolve();
  }

  setConfiguration(value) {
    assert_true(this.opened_);

    let selectedConfiguration = this.info_.configurations.find(
        configuration => configuration.configurationValue == value);
    // Blink should never request an invalid configuration.
    assert_not_equals(selectedConfiguration, undefined);
    this.currentConfiguration_ = selectedConfiguration;
    return Promise.resolve({ success: true });
  }

  claimInterface(interfaceNumber) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    assert_false(this.claimedInterfaces_.has(interfaceNumber),
                 'interface already claimed');

    // Blink should never request an invalid interface.
    assert_true(this.currentConfiguration_.interfaces.some(
            iface => iface.interfaceNumber == interfaceNumber));
    this.claimedInterfaces_.set(interfaceNumber, 0);
    return Promise.resolve({ success: true });
  }

  releaseInterface(interfaceNumber) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    assert_true(this.claimedInterfaces_.has(interfaceNumber));
    this.claimedInterfaces_.delete(interfaceNumber);
    return Promise.resolve({ success: true });
  }

  setInterfaceAlternateSetting(interfaceNumber, alternateSetting) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    assert_true(this.claimedInterfaces_.has(interfaceNumber));

    let iface = this.currentConfiguration_.interfaces.find(
        iface => iface.interfaceNumber == interfaceNumber);
    // Blink should never request an invalid interface or alternate.
    assert_false(iface == undefined);
    assert_true(iface.alternates.some(
        x => x.alternateSetting == alternateSetting));
    this.claimedInterfaces_.set(interfaceNumber, alternateSetting);
    return Promise.resolve({ success: true });
  }

  reset() {
    assert_true(this.opened_);
    return Promise.resolve({ success: true });
  }

  clearHalt(endpoint) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    return Promise.resolve({ success: true });
  }

  controlTransferIn(params, length, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      data: [length >> 8, length & 0xff, params.request, params.value >> 8,
             params.value & 0xff, params.index >> 8, params.index & 0xff]
    });
  }

  controlTransferOut(params, data, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      bytesWritten: data.byteLength
    });
  }

  genericTransferIn(endpointNumber, length, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    let data = new Array(length);
    for (let i = 0; i < length; ++i)
      data[i] = i & 0xff;
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      data: data
    });
  }

  genericTransferOut(endpointNumber, data, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      bytesWritten: data.byteLength
    });
  }

  isochronousTransferIn(endpointNumber, packetLengths, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    let data = new Array(packetLengths.reduce((a, b) => a + b, 0));
    let dataOffset = 0;
    let packets = new Array(packetLengths.length);
    for (let i = 0; i < packetLengths.length; ++i) {
      for (let j = 0; j < packetLengths[i]; ++j)
        data[dataOffset++] = j & 0xff;
      packets[i] = {
        length: packetLengths[i],
        transferredLength: packetLengths[i],
        status: device.mojom.UsbTransferStatus.OK
      };
    }
    return Promise.resolve({ data: data, packets: packets });
  }

  isochronousTransferOut(endpointNumber, data, packetLengths, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    let packets = new Array(packetLengths.length);
    for (let i = 0; i < packetLengths.length; ++i) {
      packets[i] = {
        length: packetLengths[i],
        transferredLength: packetLengths[i],
        status: device.mojom.UsbTransferStatus.OK
      };
    }
    return Promise.resolve({ packets: packets });
  }
}

class FakeWebUsbService {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.WebUsbService);
    this.devices_ = new Map();
    this.devicesByGuid_ = new Map();
    this.client_ = null;
    this.nextGuid_ = 0;
  }

  addBinding(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  addDevice(fakeDevice, info) {
    let device = {
      fakeDevice: fakeDevice,
      guid: (this.nextGuid_++).toString(),
      info: info,
      bindingArray: []
    };
    this.devices_.set(fakeDevice, device);
    this.devicesByGuid_.set(device.guid, device);
    if (this.client_)
      this.client_.onDeviceAdded(fakeDeviceInitToDeviceInfo(device.guid, info));
  }

  removeDevice(fakeDevice) {
    let device = this.devices_.get(fakeDevice);
    if (!device)
      throw new Error('Cannot remove unknown device.');

    for (var binding of device.bindingArray)
      binding.close();
    this.devices_.delete(device.fakeDevice);
    this.devicesByGuid_.delete(device.guid);
    if (this.client_) {
      this.client_.onDeviceRemoved(
          fakeDeviceInitToDeviceInfo(device.guid, device.info));
    }
  }

  removeAllDevices() {
    this.devices_.forEach(device => {
      for (var binding of device.bindingArray)
        binding.close();
      this.client_.onDeviceRemoved(
          fakeDeviceInitToDeviceInfo(device.guid, device.info));
    });
    this.devices_.clear();
    this.devicesByGuid_.clear();
  }

  getDevices() {
    let devices = [];
    this.devices_.forEach(device => {
      devices.push(fakeDeviceInitToDeviceInfo(device.guid, device.info));
    });
    return Promise.resolve({ results: devices });
  }

  getDevice(guid, request) {
    let retrievedDevice = this.devicesByGuid_.get(guid);
    if (retrievedDevice) {
      let binding = new mojo.Binding(
          device.mojom.UsbDevice,
          new FakeDevice(retrievedDevice.info),
          request);
      binding.setConnectionErrorHandler(() => {
        if (retrievedDevice.fakeDevice.onclose)
          retrievedDevice.fakeDevice.onclose();
      });
      retrievedDevice.bindingArray.push(binding);
    } else {
      request.close();
    }
  }

  getPermission(deviceFilters) {
    return new Promise(resolve => {
      if (navigator.usb.test.onrequestdevice) {
        navigator.usb.test.onrequestdevice(
            new USBDeviceRequestEvent(deviceFilters, resolve));
      } else {
        resolve({ result: null });
      }
    });
  }

  setClient(client) {
    this.client_ = client;
  }
}

class USBDeviceRequestEvent {
  constructor(deviceFilters, resolve) {
    this.filters = convertMojoDeviceFilters(deviceFilters);
    this.resolveFunc_ = resolve;
  }

  respondWith(value) {
    // Wait until |value| resolves (if it is a Promise). This function returns
    // no value.
    Promise.resolve(value).then(fakeDevice => {
      let device = internal.webUsbService.devices_.get(fakeDevice);
      let result = null;
      if (device) {
        result = fakeDeviceInitToDeviceInfo(device.guid, device.info);
      }
      this.resolveFunc_({ result: result });
    }, () => {
      this.resolveFunc_({ result: null });
    });
  }
}

// Unlike FakeDevice this class is exported to callers of USBTest.addFakeDevice.
class FakeUSBDevice {
  constructor() {
    this.onclose = null;
  }

  disconnect() {
    setTimeout(() => internal.webUsbService.removeDevice(this), 0);
  }
}

// A helper for forwarding MojoHandle instances from one frame to another.
class CrossFrameHandleProxy {
  constructor(callback) {
    let {handle0, handle1} = Mojo.createMessagePipe();
    this.sender_ = handle0;
    this.receiver_ = handle1;
    this.receiver_.watch({readable: true}, () => {
      let message = this.receiver_.readMessage();
      assert_equals(message.buffer.byteLength, 0);
      assert_equals(message.handles.length, 1);
      callback(message.handles[0]);
    });
  }

  forwardHandle(handle) {
    this.sender_.writeMessage(new ArrayBuffer, [handle]);
  }
}

class USBTest {
  constructor() {
    this.onrequestdevice = undefined;
  }

  async initialize() {
    if (internal.initialized)
      return;

    internal.webUsbService = new FakeWebUsbService();
    internal.webUsbServiceInterceptor =
        new MojoInterfaceInterceptor(blink.mojom.WebUsbService.name);
    internal.webUsbServiceInterceptor.oninterfacerequest =
        e => internal.webUsbService.addBinding(e.handle);
    internal.webUsbServiceInterceptor.start();
    internal.webUsbServiceCrossFrameProxy = new CrossFrameHandleProxy(
        handle => internal.webUsbService.addBinding(handle));

    // Wait for a call to GetDevices() to pass between the renderer and the
    // mock in order to establish that everything is set up.
    await navigator.usb.getDevices();
    internal.initialized = true;
  }

  async attachToWindow(otherWindow) {
    if (!internal.initialized)
      throw new Error('Call initialize() before attachToWindow().');

    otherWindow.webUsbServiceInterceptor =
        new otherWindow.MojoInterfaceInterceptor(
            blink.mojom.WebUsbService.name);
    otherWindow.webUsbServiceInterceptor.oninterfacerequest =
        e => internal.webUsbServiceCrossFrameProxy.forwardHandle(e.handle);
    otherWindow.webUsbServiceInterceptor.start();

    // Wait for a call to GetDevices() to pass between the renderer and the
    // mock in order to establish that everything is set up.
    await otherWindow.navigator.usb.getDevices();
  }

  addFakeDevice(deviceInit) {
    if (!internal.initialized)
      throw new Error('Call initialize() before addFakeDevice().');

    // |addDevice| and |removeDevice| are called in a setTimeout callback so
    // that tests do not rely on the device being immediately available which
    // may not be true for all implementations of this test API.
    let fakeDevice = new FakeUSBDevice();
    setTimeout(
        () => internal.webUsbService.addDevice(fakeDevice, deviceInit), 0);
    return fakeDevice;
  }

  reset() {
    if (!internal.initialized)
      throw new Error('Call initialize() before reset().');

    // Reset the mocks in a setTimeout callback so that tests do not rely on
    // the fact that this polyfill can do this synchronously.
    return new Promise(resolve => {
      setTimeout(() => {
        internal.webUsbService.removeAllDevices();
        resolve();
      }, 0);
    });
  }
}

navigator.usb.test = new USBTest();

})();
