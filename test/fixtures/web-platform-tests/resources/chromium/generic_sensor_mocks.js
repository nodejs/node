'use strict';

var GenericSensorTest = (() => {
  // Class that mocks Sensor interface defined in
  // https://cs.chromium.org/chromium/src/services/device/public/mojom/sensor.mojom
  class MockSensor {
    constructor(sensorRequest, handle, offset, size, reportingMode) {
      this.client_ = null;
      this.reportingMode_ = reportingMode;
      this.sensorReadingTimerId_ = null;
      this.requestedFrequencies_ = [];
      let rv = handle.mapBuffer(offset, size);
      assert_equals(rv.result, Mojo.RESULT_OK, "Failed to map shared buffer");
      this.buffer_ = new Float64Array(rv.buffer);
      this.buffer_.fill(0);
      this.binding_ = new mojo.Binding(device.mojom.Sensor, this,
                                       sensorRequest);
      this.binding_.setConnectionErrorHandler(() => {
        this.reset();
      });
    }

    getDefaultConfiguration() {
      return Promise.resolve({frequency: 5});
    }

    addConfiguration(configuration) {
      assert_not_equals(configuration, null, "Invalid sensor configuration.");

      this.requestedFrequencies_.push(configuration.frequency);
      // Sort using descending order.
      this.requestedFrequencies_.sort(
          (first, second) => { return second - first });

      this.startReading();

      return Promise.resolve({success: true});
    }

    removeConfiguration(configuration) {
      let index = this.requestedFrequencies_.indexOf(configuration.frequency);
      if (index == -1)
        return;

      this.requestedFrequencies_.splice(index, 1);

      if (this.isReading) {
        this.stopReading();
        if (this.requestedFrequencies_.length !== 0)
          this.startReading();
      }
    }

    suspend() {
      this.stopReading();
    }

    resume() {
      this.startReading();
    }

    reset() {
      this.stopReading();
      this.requestedFrequencies_ = [];
      this.buffer_.fill(0);
      this.binding_.close();
    }

    startReading() {
      if (this.isReading) {
        console.warn("Sensor reading is already started.");
        return;
      }

      if (this.requestedFrequencies_.length == 0) {
        console.warn("Sensor reading cannot be started as" +
                     "there are no configurations added.");
        return;
      }

      const maxFrequencyHz = this.requestedFrequencies_[0];
      const timeoutMs = (1 / maxFrequencyHz) * 1000;
      this.sensorReadingTimerId_ = window.setInterval(() => {
        // For all tests sensor reading should have monotonically
        // increasing timestamp in seconds.
        this.buffer_[1] = window.performance.now() * 0.001;
        if (this.reportingMode_ === device.mojom.ReportingMode.ON_CHANGE) {
          this.client_.sensorReadingChanged();
        }
      }, timeoutMs);
    }

    stopReading() {
      if (this.isReading) {
        window.clearInterval(this.sensorReadingTimerId_);
        this.sensorReadingTimerId_ = null;
      }
    }

     get isReading() {
       this.sensorReadingTimerId_ !== null;
     }
  }

  // Class that mocks SensorProvider interface defined in
  // https://cs.chromium.org/chromium/src/services/device/public/mojom/sensor_provider.mojom
  class MockSensorProvider {
    constructor() {
      this.readingSizeInBytes_ =
          device.mojom.SensorInitParams.kReadBufferSizeForTests;
      this.sharedBufferSizeInBytes_ = this.readingSizeInBytes_ *
              device.mojom.SensorType.LAST;
      let rv = Mojo.createSharedBuffer(this.sharedBufferSizeInBytes_);
      assert_equals(rv.result, Mojo.RESULT_OK, "Failed to create buffer");
      this.sharedBufferHandle_ = rv.handle;
      this.activeSensor_ = null;
      this.isContinuous_ = false;
      this.binding_ = new mojo.Binding(device.mojom.SensorProvider, this);

      this.interceptor_ = new MojoInterfaceInterceptor(
          device.mojom.SensorProvider.name);
      this.interceptor_.oninterfacerequest = e => {
        this.binding_.bind(e.handle);
        this.binding_.setConnectionErrorHandler(() => {
          console.error("Mojo connection error");
          this.reset();
        });
      };
      this.interceptor_.start();
    }

    async getSensor(type) {
      const offset = (device.mojom.SensorType.LAST - type) *
                      this.readingSizeInBytes_;
      const reportingMode = device.mojom.ReportingMode.ON_CHANGE;

      let sensorPtr = new device.mojom.SensorPtr();
      if (this.activeSensor_ == null) {
        let mockSensor = new MockSensor(
            mojo.makeRequest(sensorPtr), this.sharedBufferHandle_, offset,
            this.readingSizeInBytes_, reportingMode);
        this.activeSensor_ = mockSensor;
        this.activeSensor_.client_ = new device.mojom.SensorClientPtr();
      }

      let rv = this.sharedBufferHandle_.duplicateBufferHandle();

      assert_equals(rv.result, Mojo.RESULT_OK);
      let maxAllowedFrequencyHz = 60;
      if (type == device.mojom.SensorType.AMBIENT_LIGHT ||
          type == device.mojom.SensorType.MAGNETOMETER) {
        maxAllowedFrequencyHz = 10;
      }

      let initParams = new device.mojom.SensorInitParams({
        sensor: sensorPtr,
        clientRequest: mojo.makeRequest(this.activeSensor_.client_),
        memory: rv.handle,
        bufferOffset: offset,
        mode: reportingMode,
        defaultConfiguration: {frequency: 5},
        minimumFrequency: 1,
        maximumFrequency: maxAllowedFrequencyHz
      });

      return {result: device.mojom.SensorCreationResult.SUCCESS,
              initParams: initParams};
    }

    reset() {
      if (this.activeSensor_ !== null) {
        this.activeSensor_.reset();
        this.activeSensor_ = null;
      }
      this.binding_.close();
      this.interceptor_.stop();
    }
  }

  let testInternal = {
    initialized: false,
    sensorProvider: null
  }

  class GenericSensorTestChromium {
    constructor() {
      Object.freeze(this); // Make it immutable.
    }

    initialize() {
      if (testInternal.initialized)
        throw new Error('Call reset() before initialize().');

      if (window.testRunner) { // Grant sensor permissions for Chromium testrunner.
        ['accelerometer', 'gyroscope',
         'magnetometer', 'ambient-light-sensor'].forEach((entry) => {
          window.testRunner.setPermission(entry, 'granted',
                                          location.origin, location.origin);
        });
      }

      testInternal.sensorProvider = new MockSensorProvider;
      testInternal.initialized = true;
    }
    // Resets state of sensor mocks between test runs.
    async reset() {
      if (!testInternal.initialized)
        throw new Error('Call initialize() before reset().');
      testInternal.sensorProvider.reset();
      testInternal.sensorProvider = null;
      testInternal.initialized = false;

      // Wait for an event loop iteration to let any pending mojo commands in
      // the sensor provider finish.
      await new Promise(resolve => setTimeout(resolve, 0));
    }
  }

  return GenericSensorTestChromium;
})();
