// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if (mojo && mojo.internal) {
  throw new Error('The Mojo bindings library has been initialized.');
}

var mojo = mojo || {};
mojo.internal = {};
mojo.internal.global = this;
mojo.config = {
  // Whether to automatically load mojom dependencies.
  // For example, if foo.mojom imports bar.mojom, |autoLoadMojomDeps| set to
  // true means that loading foo.mojom.js will insert a <script> tag to load
  // bar.mojom.js, if it hasn't been loaded.
  //
  // The URL of bar.mojom.js is determined by the relative path of bar.mojom
  // (relative to the position of foo.mojom at build time) and the URL of
  // foo.mojom.js. For exmple, if at build time the two mojom files are
  // located at:
  //   a/b/c/foo.mojom
  //   a/b/d/bar.mojom
  // and the URL of foo.mojom.js is:
  //   http://example.org/scripts/b/c/foo.mojom.js
  // then the URL of bar.mojom.js will be:
  //   http://example.org/scripts/b/d/bar.mojom.js
  //
  // If you would like bar.mojom.js to live at a different location, you need
  // to turn off |autoLoadMojomDeps| before loading foo.mojom.js, and manually
  // load bar.mojom.js yourself. Similarly, you need to turn off the option if
  // you merge bar.mojom.js and foo.mojom.js into a single file.
  //
  // Performance tip: Avoid loading the same mojom.js file multiple times.
  // Assume that |autoLoadMojomDeps| is set to true,
  //
  // <!--
  // (This comment tag is necessary on IOS to avoid interpreting the closing
  // script tags in the example.)
  //
  // No duplicate loading; recommended:
  // <script src="http://example.org/scripts/b/c/foo.mojom.js"></script>
  //
  // No duplicate loading, although unnecessary:
  // <script src="http://example.org/scripts/b/d/bar.mojom.js"></script>
  // <script src="http://example.org/scripts/b/c/foo.mojom.js"></script>
  //
  // Load bar.mojom.js twice; should be avoided:
  // <script src="http://example.org/scripts/b/c/foo.mojom.js"></script>
  // <script src="http://example.org/scripts/b/d/bar.mojom.js"></script>
  //
  // -->
  autoLoadMojomDeps: true
};

(function() {
  var internal = mojo.internal;

  var LoadState = {
    PENDING_LOAD: 1,
    LOADED: 2
  };

  var mojomRegistry = new Map();

  function exposeNamespace(namespace) {
    var current = internal.global;
    var parts = namespace.split('.');

    for (var part; parts.length && (part = parts.shift());) {
      if (!current[part]) {
        current[part] = {};
      }
      current = current[part];
    }

    return current;
  }

  function isMojomPendingLoad(id) {
    return mojomRegistry.get(id) === LoadState.PENDING_LOAD;
  }

  function isMojomLoaded(id) {
    return mojomRegistry.get(id) === LoadState.LOADED;
  }

  function markMojomPendingLoad(id) {
    if (isMojomLoaded(id)) {
      throw new Error('The following mojom file has been loaded: ' + id);
    }

    mojomRegistry.set(id, LoadState.PENDING_LOAD);
  }

  function markMojomLoaded(id) {
    mojomRegistry.set(id, LoadState.LOADED);
  }

  function loadMojomIfNecessary(id, relativePath) {
    if (mojomRegistry.has(id)) {
      return;
    }

    if (internal.global.document === undefined) {
      throw new Error(
          'Mojom dependency autoloading is not implemented in workers. ' +
          'Please see config variable mojo.config.autoLoadMojomDeps for more ' +
          'details.');
    }

    markMojomPendingLoad(id);
    var url = new URL(relativePath, document.currentScript.src).href;
    internal.global.document.write('<script type="text/javascript" src="' +
                                   url + '"><' + '/script>');
  }

  internal.exposeNamespace = exposeNamespace;
  internal.isMojomPendingLoad = isMojomPendingLoad;
  internal.isMojomLoaded = isMojomLoaded;
  internal.markMojomPendingLoad = markMojomPendingLoad;
  internal.markMojomLoaded = markMojomLoaded;
  internal.loadMojomIfNecessary = loadMojomIfNecessary;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  // ---------------------------------------------------------------------------

  // |output| could be an interface pointer, InterfacePtrInfo or
  // AssociatedInterfacePtrInfo.
  function makeRequest(output) {
    if (output instanceof mojo.AssociatedInterfacePtrInfo) {
      var {handle0, handle1} = internal.createPairPendingAssociation();
      output.interfaceEndpointHandle = handle0;
      output.version = 0;

      return new mojo.AssociatedInterfaceRequest(handle1);
    }

    if (output instanceof mojo.InterfacePtrInfo) {
      var pipe = Mojo.createMessagePipe();
      output.handle = pipe.handle0;
      output.version = 0;

      return new mojo.InterfaceRequest(pipe.handle1);
    }

    var pipe = Mojo.createMessagePipe();
    output.ptr.bind(new mojo.InterfacePtrInfo(pipe.handle0, 0));
    return new mojo.InterfaceRequest(pipe.handle1);
  }

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an interface pointer. Exposed as the
  // |ptr| field of generated interface pointer classes.
  // |ptrInfoOrHandle| could be omitted and passed into bind() later.
  function InterfacePtrController(interfaceType, ptrInfoOrHandle) {
    this.version = 0;

    this.interfaceType_ = interfaceType;
    this.router_ = null;
    this.interfaceEndpointClient_ = null;
    this.proxy_ = null;

    // |router_| and |interfaceEndpointClient_| are lazily initialized.
    // |handle_| is valid between bind() and
    // the initialization of |router_| and |interfaceEndpointClient_|.
    this.handle_ = null;

    if (ptrInfoOrHandle)
      this.bind(ptrInfoOrHandle);
  }

  InterfacePtrController.prototype.bind = function(ptrInfoOrHandle) {
    this.reset();

    if (ptrInfoOrHandle instanceof mojo.InterfacePtrInfo) {
      this.version = ptrInfoOrHandle.version;
      this.handle_ = ptrInfoOrHandle.handle;
    } else {
      this.handle_ = ptrInfoOrHandle;
    }
  };

  InterfacePtrController.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null || this.handle_ !== null;
  };

  // Although users could just discard the object, reset() closes the pipe
  // immediately.
  InterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }
    if (this.router_) {
      this.router_.close();
      this.router_ = null;

      this.proxy_ = null;
    }
    if (this.handle_) {
      this.handle_.close();
      this.handle_ = null;
    }
  };

  InterfacePtrController.prototype.resetWithReason = function(reason) {
    if (this.isBound()) {
      this.configureProxyIfNecessary_();
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.reset();
  };

  InterfacePtrController.prototype.setConnectionErrorHandler = function(
      callback) {
    if (!this.isBound())
      throw new Error("Cannot set connection error handler if not bound.");

    this.configureProxyIfNecessary_();
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  InterfacePtrController.prototype.passInterface = function() {
    var result;
    if (this.router_) {
      // TODO(yzshen): Fix Router interface to support extracting handle.
      result = new mojo.InterfacePtrInfo(
          this.router_.connector_.handle_, this.version);
      this.router_.connector_.handle_ = null;
    } else {
      // This also handles the case when this object is not bound.
      result = new mojo.InterfacePtrInfo(this.handle_, this.version);
      this.handle_ = null;
    }

    this.reset();
    return result;
  };

  InterfacePtrController.prototype.getProxy = function() {
    this.configureProxyIfNecessary_();
    return this.proxy_;
  };

  InterfacePtrController.prototype.configureProxyIfNecessary_ = function() {
    if (!this.handle_)
      return;

    this.router_ = new internal.Router(this.handle_, true);
    this.handle_ = null;

    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        this.router_.createLocalEndpointHandle(internal.kMasterInterfaceId));

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateResponse]);
    this.proxy_ = new this.interfaceType_.proxyClass(
        this.interfaceEndpointClient_);
  };

  InterfacePtrController.prototype.queryVersion = function() {
    function onQueryVersion(version) {
      this.version = version;
      return version;
    }

    this.configureProxyIfNecessary_();
    return this.interfaceEndpointClient_.queryVersion().then(
      onQueryVersion.bind(this));
  };

  InterfacePtrController.prototype.requireVersion = function(version) {
    this.configureProxyIfNecessary_();

    if (this.version >= version) {
      return;
    }
    this.version = version;
    this.interfaceEndpointClient_.requireVersion(version);
  };

  // ---------------------------------------------------------------------------

  // |request| could be omitted and passed into bind() later.
  //
  // Example:
  //
  //    // FooImpl implements mojom.Foo.
  //    function FooImpl() { ... }
  //    FooImpl.prototype.fooMethod1 = function() { ... }
  //    FooImpl.prototype.fooMethod2 = function() { ... }
  //
  //    var fooPtr = new mojom.FooPtr();
  //    var request = makeRequest(fooPtr);
  //    var binding = new Binding(mojom.Foo, new FooImpl(), request);
  //    fooPtr.fooMethod1();
  function Binding(interfaceType, impl, requestOrHandle) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.router_ = null;
    this.interfaceEndpointClient_ = null;
    this.stub_ = null;

    if (requestOrHandle)
      this.bind(requestOrHandle);
  }

  Binding.prototype.isBound = function() {
    return this.router_ !== null;
  };

  Binding.prototype.createInterfacePtrAndBind = function() {
    var ptr = new this.interfaceType_.ptrClass();
    // TODO(yzshen): Set the version of the interface pointer.
    this.bind(makeRequest(ptr));
    return ptr;
  };

  Binding.prototype.bind = function(requestOrHandle) {
    this.close();

    var handle = requestOrHandle instanceof mojo.InterfaceRequest ?
        requestOrHandle.handle : requestOrHandle;
    if (!(handle instanceof MojoHandle))
      return;

    this.router_ = new internal.Router(handle);

    this.stub_ = new this.interfaceType_.stubClass(this.impl_);
    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        this.router_.createLocalEndpointHandle(internal.kMasterInterfaceId),
        this.stub_, this.interfaceType_.kVersion);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateRequest]);
  };

  Binding.prototype.close = function() {
    if (!this.isBound())
      return;

    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }

    this.router_.close();
    this.router_ = null;
    this.stub_ = null;
  };

  Binding.prototype.closeWithReason = function(reason) {
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.close();
  };

  Binding.prototype.setConnectionErrorHandler = function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  Binding.prototype.unbind = function() {
    if (!this.isBound())
      return new mojo.InterfaceRequest(null);

    var result = new mojo.InterfaceRequest(this.router_.connector_.handle_);
    this.router_.connector_.handle_ = null;
    this.close();
    return result;
  };

  // ---------------------------------------------------------------------------

  function BindingSetEntry(bindingSet, interfaceType, bindingType, impl,
      requestOrHandle, bindingId) {
    this.bindingSet_ = bindingSet;
    this.bindingId_ = bindingId;
    this.binding_ = new bindingType(interfaceType, impl,
        requestOrHandle);

    this.binding_.setConnectionErrorHandler(function(reason) {
      this.bindingSet_.onConnectionError(bindingId, reason);
    }.bind(this));
  }

  BindingSetEntry.prototype.close = function() {
    this.binding_.close();
  };

  function BindingSet(interfaceType) {
    this.interfaceType_ = interfaceType;
    this.nextBindingId_ = 0;
    this.bindings_ = new Map();
    this.errorHandler_ = null;
    this.bindingType_ = Binding;
  }

  BindingSet.prototype.isEmpty = function() {
    return this.bindings_.size == 0;
  };

  BindingSet.prototype.addBinding = function(impl, requestOrHandle) {
    this.bindings_.set(
        this.nextBindingId_,
        new BindingSetEntry(this, this.interfaceType_, this.bindingType_, impl,
            requestOrHandle, this.nextBindingId_));
    ++this.nextBindingId_;
  };

  BindingSet.prototype.closeAllBindings = function() {
    for (var entry of this.bindings_.values())
      entry.close();
    this.bindings_.clear();
  };

  BindingSet.prototype.setConnectionErrorHandler = function(callback) {
    this.errorHandler_ = callback;
  };

  BindingSet.prototype.onConnectionError = function(bindingId, reason) {
    this.bindings_.delete(bindingId);

    if (this.errorHandler_)
      this.errorHandler_(reason);
  };

  // ---------------------------------------------------------------------------

  // Operations used to setup/configure an associated interface pointer.
  // Exposed as |ptr| field of generated associated interface pointer classes.
  // |associatedPtrInfo| could be omitted and passed into bind() later.
  //
  // Example:
  //    // IntegerSenderImpl implements mojom.IntegerSender
  //    function IntegerSenderImpl() { ... }
  //    IntegerSenderImpl.prototype.echo = function() { ... }
  //
  //    // IntegerSenderConnectionImpl implements mojom.IntegerSenderConnection
  //    function IntegerSenderConnectionImpl() {
  //      this.senderBinding_ = null;
  //    }
  //    IntegerSenderConnectionImpl.prototype.getSender = function(
  //        associatedRequest) {
  //      this.senderBinding_ = new AssociatedBinding(mojom.IntegerSender,
  //          new IntegerSenderImpl(),
  //          associatedRequest);
  //    }
  //
  //    var integerSenderConnection = new mojom.IntegerSenderConnectionPtr();
  //    var integerSenderConnectionBinding = new Binding(
  //        mojom.IntegerSenderConnection,
  //        new IntegerSenderConnectionImpl(),
  //        mojo.makeRequest(integerSenderConnection));
  //
  //    // A locally-created associated interface pointer can only be used to
  //    // make calls when the corresponding associated request is sent over
  //    // another interface (either the master interface or another
  //    // associated interface).
  //    var associatedInterfacePtrInfo = new AssociatedInterfacePtrInfo();
  //    var associatedRequest = makeRequest(interfacePtrInfo);
  //
  //    integerSenderConnection.getSender(associatedRequest);
  //
  //    // Create an associated interface and bind the associated handle.
  //    var integerSender = new mojom.AssociatedIntegerSenderPtr();
  //    integerSender.ptr.bind(associatedInterfacePtrInfo);
  //    integerSender.echo();

  function AssociatedInterfacePtrController(interfaceType, associatedPtrInfo) {
    this.version = 0;

    this.interfaceType_ = interfaceType;
    this.interfaceEndpointClient_ = null;
    this.proxy_ = null;

    if (associatedPtrInfo) {
      this.bind(associatedPtrInfo);
    }
  }

  AssociatedInterfacePtrController.prototype.bind = function(
      associatedPtrInfo) {
    this.reset();
    this.version = associatedPtrInfo.version;

    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        associatedPtrInfo.interfaceEndpointHandle);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateResponse]);
    this.proxy_ = new this.interfaceType_.proxyClass(
        this.interfaceEndpointClient_);
  };

  AssociatedInterfacePtrController.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null;
  };

  AssociatedInterfacePtrController.prototype.reset = function() {
    this.version = 0;
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }
    if (this.proxy_) {
      this.proxy_ = null;
    }
  };

  AssociatedInterfacePtrController.prototype.resetWithReason = function(
      reason) {
    if (this.isBound()) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.reset();
  };

  // Indicates whether an error has been encountered. If true, method calls
  // on this interface will be dropped (and may already have been dropped).
  AssociatedInterfacePtrController.prototype.getEncounteredError = function() {
    return this.interfaceEndpointClient_ ?
        this.interfaceEndpointClient_.getEncounteredError() : false;
  };

  AssociatedInterfacePtrController.prototype.setConnectionErrorHandler =
      function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }

    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  AssociatedInterfacePtrController.prototype.passInterface = function() {
    if (!this.isBound()) {
      return new mojo.AssociatedInterfacePtrInfo(null);
    }

    var result = new mojo.AssociatedInterfacePtrInfo(
        this.interfaceEndpointClient_.passHandle(), this.version);
    this.reset();
    return result;
  };

  AssociatedInterfacePtrController.prototype.getProxy = function() {
    return this.proxy_;
  };

  AssociatedInterfacePtrController.prototype.queryVersion = function() {
    function onQueryVersion(version) {
      this.version = version;
      return version;
    }

    return this.interfaceEndpointClient_.queryVersion().then(
      onQueryVersion.bind(this));
  };

  AssociatedInterfacePtrController.prototype.requireVersion = function(
      version) {
    if (this.version >= version) {
      return;
    }
    this.version = version;
    this.interfaceEndpointClient_.requireVersion(version);
  };

  // ---------------------------------------------------------------------------

  // |associatedInterfaceRequest| could be omitted and passed into bind()
  // later.
  function AssociatedBinding(interfaceType, impl, associatedInterfaceRequest) {
    this.interfaceType_ = interfaceType;
    this.impl_ = impl;
    this.interfaceEndpointClient_ = null;
    this.stub_ = null;

    if (associatedInterfaceRequest) {
      this.bind(associatedInterfaceRequest);
    }
  }

  AssociatedBinding.prototype.isBound = function() {
    return this.interfaceEndpointClient_ !== null;
  };

  AssociatedBinding.prototype.bind = function(associatedInterfaceRequest) {
    this.close();

    this.stub_ = new this.interfaceType_.stubClass(this.impl_);
    this.interfaceEndpointClient_ = new internal.InterfaceEndpointClient(
        associatedInterfaceRequest.interfaceEndpointHandle, this.stub_,
        this.interfaceType_.kVersion);

    this.interfaceEndpointClient_ .setPayloadValidators([
        this.interfaceType_.validateRequest]);
  };


  AssociatedBinding.prototype.close = function() {
    if (!this.isBound()) {
      return;
    }

    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close();
      this.interfaceEndpointClient_ = null;
    }

    this.stub_ = null;
  };

  AssociatedBinding.prototype.closeWithReason = function(reason) {
    if (this.interfaceEndpointClient_) {
      this.interfaceEndpointClient_.close(reason);
      this.interfaceEndpointClient_ = null;
    }
    this.close();
  };

  AssociatedBinding.prototype.setConnectionErrorHandler = function(callback) {
    if (!this.isBound()) {
      throw new Error("Cannot set connection error handler if not bound.");
    }
    this.interfaceEndpointClient_.setConnectionErrorHandler(callback);
  };

  AssociatedBinding.prototype.unbind = function() {
    if (!this.isBound()) {
      return new mojo.AssociatedInterfaceRequest(null);
    }

    var result = new mojo.AssociatedInterfaceRequest(
        this.interfaceEndpointClient_.passHandle());
    this.close();
    return result;
  };

  // ---------------------------------------------------------------------------

  function AssociatedBindingSet(interfaceType) {
    mojo.BindingSet.call(this, interfaceType);
    this.bindingType_ = AssociatedBinding;
  }

  AssociatedBindingSet.prototype = Object.create(BindingSet.prototype);
  AssociatedBindingSet.prototype.constructor = AssociatedBindingSet;

  mojo.makeRequest = makeRequest;
  mojo.AssociatedInterfacePtrController = AssociatedInterfacePtrController;
  mojo.AssociatedBinding = AssociatedBinding;
  mojo.AssociatedBindingSet = AssociatedBindingSet;
  mojo.Binding = Binding;
  mojo.BindingSet = BindingSet;
  mojo.InterfacePtrController = InterfacePtrController;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  var kHostIsLittleEndian = (function () {
    var endianArrayBuffer = new ArrayBuffer(2);
    var endianUint8Array = new Uint8Array(endianArrayBuffer);
    var endianUint16Array = new Uint16Array(endianArrayBuffer);
    endianUint16Array[0] = 1;
    return endianUint8Array[0] == 1;
  })();

  var kHighWordMultiplier = 0x100000000;

  function Buffer(sizeOrArrayBuffer) {
    if (sizeOrArrayBuffer instanceof ArrayBuffer)
      this.arrayBuffer = sizeOrArrayBuffer;
    else
      this.arrayBuffer = new ArrayBuffer(sizeOrArrayBuffer);

    this.dataView = new DataView(this.arrayBuffer);
    this.next = 0;
  }

  Object.defineProperty(Buffer.prototype, "byteLength", {
    get: function() { return this.arrayBuffer.byteLength; }
  });

  Buffer.prototype.alloc = function(size) {
    var pointer = this.next;
    this.next += size;
    if (this.next > this.byteLength) {
      var newSize = (1.5 * (this.byteLength + size)) | 0;
      this.grow(newSize);
    }
    return pointer;
  };

  function copyArrayBuffer(dstArrayBuffer, srcArrayBuffer) {
    (new Uint8Array(dstArrayBuffer)).set(new Uint8Array(srcArrayBuffer));
  }

  Buffer.prototype.grow = function(size) {
    var newArrayBuffer = new ArrayBuffer(size);
    copyArrayBuffer(newArrayBuffer, this.arrayBuffer);
    this.arrayBuffer = newArrayBuffer;
    this.dataView = new DataView(this.arrayBuffer);
  };

  Buffer.prototype.trim = function() {
    this.arrayBuffer = this.arrayBuffer.slice(0, this.next);
    this.dataView = new DataView(this.arrayBuffer);
  };

  Buffer.prototype.getUint8 = function(offset) {
    return this.dataView.getUint8(offset);
  }
  Buffer.prototype.getUint16 = function(offset) {
    return this.dataView.getUint16(offset, kHostIsLittleEndian);
  }
  Buffer.prototype.getUint32 = function(offset) {
    return this.dataView.getUint32(offset, kHostIsLittleEndian);
  }
  Buffer.prototype.getUint64 = function(offset) {
    var lo, hi;
    if (kHostIsLittleEndian) {
      lo = this.dataView.getUint32(offset, kHostIsLittleEndian);
      hi = this.dataView.getUint32(offset + 4, kHostIsLittleEndian);
    } else {
      hi = this.dataView.getUint32(offset, kHostIsLittleEndian);
      lo = this.dataView.getUint32(offset + 4, kHostIsLittleEndian);
    }
    return lo + hi * kHighWordMultiplier;
  }

  Buffer.prototype.getInt8 = function(offset) {
    return this.dataView.getInt8(offset);
  }
  Buffer.prototype.getInt16 = function(offset) {
    return this.dataView.getInt16(offset, kHostIsLittleEndian);
  }
  Buffer.prototype.getInt32 = function(offset) {
    return this.dataView.getInt32(offset, kHostIsLittleEndian);
  }
  Buffer.prototype.getInt64 = function(offset) {
    var lo, hi;
    if (kHostIsLittleEndian) {
      lo = this.dataView.getUint32(offset, kHostIsLittleEndian);
      hi = this.dataView.getInt32(offset + 4, kHostIsLittleEndian);
    } else {
      hi = this.dataView.getInt32(offset, kHostIsLittleEndian);
      lo = this.dataView.getUint32(offset + 4, kHostIsLittleEndian);
    }
    return lo + hi * kHighWordMultiplier;
  }

  Buffer.prototype.getFloat32 = function(offset) {
    return this.dataView.getFloat32(offset, kHostIsLittleEndian);
  }
  Buffer.prototype.getFloat64 = function(offset) {
    return this.dataView.getFloat64(offset, kHostIsLittleEndian);
  }

  Buffer.prototype.setUint8 = function(offset, value) {
    this.dataView.setUint8(offset, value);
  }
  Buffer.prototype.setUint16 = function(offset, value) {
    this.dataView.setUint16(offset, value, kHostIsLittleEndian);
  }
  Buffer.prototype.setUint32 = function(offset, value) {
    this.dataView.setUint32(offset, value, kHostIsLittleEndian);
  }
  Buffer.prototype.setUint64 = function(offset, value) {
    var hi = (value / kHighWordMultiplier) | 0;
    if (kHostIsLittleEndian) {
      this.dataView.setInt32(offset, value, kHostIsLittleEndian);
      this.dataView.setInt32(offset + 4, hi, kHostIsLittleEndian);
    } else {
      this.dataView.setInt32(offset, hi, kHostIsLittleEndian);
      this.dataView.setInt32(offset + 4, value, kHostIsLittleEndian);
    }
  }

  Buffer.prototype.setInt8 = function(offset, value) {
    this.dataView.setInt8(offset, value);
  }
  Buffer.prototype.setInt16 = function(offset, value) {
    this.dataView.setInt16(offset, value, kHostIsLittleEndian);
  }
  Buffer.prototype.setInt32 = function(offset, value) {
    this.dataView.setInt32(offset, value, kHostIsLittleEndian);
  }
  Buffer.prototype.setInt64 = function(offset, value) {
    var hi = Math.floor(value / kHighWordMultiplier);
    if (kHostIsLittleEndian) {
      this.dataView.setInt32(offset, value, kHostIsLittleEndian);
      this.dataView.setInt32(offset + 4, hi, kHostIsLittleEndian);
    } else {
      this.dataView.setInt32(offset, hi, kHostIsLittleEndian);
      this.dataView.setInt32(offset + 4, value, kHostIsLittleEndian);
    }
  }

  Buffer.prototype.setFloat32 = function(offset, value) {
    this.dataView.setFloat32(offset, value, kHostIsLittleEndian);
  }
  Buffer.prototype.setFloat64 = function(offset, value) {
    this.dataView.setFloat64(offset, value, kHostIsLittleEndian);
  }

  internal.Buffer = Buffer;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  var kErrorUnsigned = "Passing negative value to unsigned";
  var kErrorArray = "Passing non Array for array type";
  var kErrorString = "Passing non String for string type";
  var kErrorMap = "Passing non Map for map type";

  // Memory -------------------------------------------------------------------

  var kAlignment = 8;

  function align(size) {
    return size + (kAlignment - (size % kAlignment)) % kAlignment;
  }

  function isAligned(offset) {
    return offset >= 0 && (offset % kAlignment) === 0;
  }

  // Constants ----------------------------------------------------------------

  var kArrayHeaderSize = 8;
  var kStructHeaderSize = 8;
  var kMessageV0HeaderSize = 24;
  var kMessageV1HeaderSize = 32;
  var kMessageV2HeaderSize = 48;
  var kMapStructPayloadSize = 16;

  var kStructHeaderNumBytesOffset = 0;
  var kStructHeaderVersionOffset = 4;

  var kEncodedInvalidHandleValue = 0xFFFFFFFF;

  // Decoder ------------------------------------------------------------------

  function Decoder(buffer, handles, associatedEndpointHandles, base) {
    this.buffer = buffer;
    this.handles = handles;
    this.associatedEndpointHandles = associatedEndpointHandles;
    this.base = base;
    this.next = base;
  }

  Decoder.prototype.align = function() {
    this.next = align(this.next);
  };

  Decoder.prototype.skip = function(offset) {
    this.next += offset;
  };

  Decoder.prototype.readInt8 = function() {
    var result = this.buffer.getInt8(this.next);
    this.next += 1;
    return result;
  };

  Decoder.prototype.readUint8 = function() {
    var result = this.buffer.getUint8(this.next);
    this.next += 1;
    return result;
  };

  Decoder.prototype.readInt16 = function() {
    var result = this.buffer.getInt16(this.next);
    this.next += 2;
    return result;
  };

  Decoder.prototype.readUint16 = function() {
    var result = this.buffer.getUint16(this.next);
    this.next += 2;
    return result;
  };

  Decoder.prototype.readInt32 = function() {
    var result = this.buffer.getInt32(this.next);
    this.next += 4;
    return result;
  };

  Decoder.prototype.readUint32 = function() {
    var result = this.buffer.getUint32(this.next);
    this.next += 4;
    return result;
  };

  Decoder.prototype.readInt64 = function() {
    var result = this.buffer.getInt64(this.next);
    this.next += 8;
    return result;
  };

  Decoder.prototype.readUint64 = function() {
    var result = this.buffer.getUint64(this.next);
    this.next += 8;
    return result;
  };

  Decoder.prototype.readFloat = function() {
    var result = this.buffer.getFloat32(this.next);
    this.next += 4;
    return result;
  };

  Decoder.prototype.readDouble = function() {
    var result = this.buffer.getFloat64(this.next);
    this.next += 8;
    return result;
  };

  Decoder.prototype.decodePointer = function() {
    // TODO(abarth): To correctly decode a pointer, we need to know the real
    // base address of the array buffer.
    var offsetPointer = this.next;
    var offset = this.readUint64();
    if (!offset)
      return 0;
    return offsetPointer + offset;
  };

  Decoder.prototype.decodeAndCreateDecoder = function(pointer) {
    return new Decoder(this.buffer, this.handles,
        this.associatedEndpointHandles, pointer);
  };

  Decoder.prototype.decodeHandle = function() {
    return this.handles[this.readUint32()] || null;
  };

  Decoder.prototype.decodeAssociatedEndpointHandle = function() {
    return this.associatedEndpointHandles[this.readUint32()] || null;
  };

  Decoder.prototype.decodeString = function() {
    var numberOfBytes = this.readUint32();
    var numberOfElements = this.readUint32();
    var base = this.next;
    this.next += numberOfElements;
    return internal.decodeUtf8String(
        new Uint8Array(this.buffer.arrayBuffer, base, numberOfElements));
  };

  Decoder.prototype.decodeArray = function(cls) {
    var numberOfBytes = this.readUint32();
    var numberOfElements = this.readUint32();
    var val = new Array(numberOfElements);
    if (cls === PackedBool) {
      var byte;
      for (var i = 0; i < numberOfElements; ++i) {
        if (i % 8 === 0)
          byte = this.readUint8();
        val[i] = (byte & (1 << i % 8)) ? true : false;
      }
    } else {
      for (var i = 0; i < numberOfElements; ++i) {
        val[i] = cls.decode(this);
      }
    }
    return val;
  };

  Decoder.prototype.decodeStruct = function(cls) {
    return cls.decode(this);
  };

  Decoder.prototype.decodeStructPointer = function(cls) {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    return cls.decode(this.decodeAndCreateDecoder(pointer));
  };

  Decoder.prototype.decodeArrayPointer = function(cls) {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    return this.decodeAndCreateDecoder(pointer).decodeArray(cls);
  };

  Decoder.prototype.decodeStringPointer = function() {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    return this.decodeAndCreateDecoder(pointer).decodeString();
  };

  Decoder.prototype.decodeMap = function(keyClass, valueClass) {
    this.skip(4); // numberOfBytes
    this.skip(4); // version
    var keys = this.decodeArrayPointer(keyClass);
    var values = this.decodeArrayPointer(valueClass);
    var val = new Map();
    for (var i = 0; i < keys.length; i++)
      val.set(keys[i], values[i]);
    return val;
  };

  Decoder.prototype.decodeMapPointer = function(keyClass, valueClass) {
    var pointer = this.decodePointer();
    if (!pointer) {
      return null;
    }
    var decoder = this.decodeAndCreateDecoder(pointer);
    return decoder.decodeMap(keyClass, valueClass);
  };

  // Encoder ------------------------------------------------------------------

  function Encoder(buffer, handles, associatedEndpointHandles, base) {
    this.buffer = buffer;
    this.handles = handles;
    this.associatedEndpointHandles = associatedEndpointHandles;
    this.base = base;
    this.next = base;
  }

  Encoder.prototype.align = function() {
    this.next = align(this.next);
  };

  Encoder.prototype.skip = function(offset) {
    this.next += offset;
  };

  Encoder.prototype.writeInt8 = function(val) {
    this.buffer.setInt8(this.next, val);
    this.next += 1;
  };

  Encoder.prototype.writeUint8 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    this.buffer.setUint8(this.next, val);
    this.next += 1;
  };

  Encoder.prototype.writeInt16 = function(val) {
    this.buffer.setInt16(this.next, val);
    this.next += 2;
  };

  Encoder.prototype.writeUint16 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    this.buffer.setUint16(this.next, val);
    this.next += 2;
  };

  Encoder.prototype.writeInt32 = function(val) {
    this.buffer.setInt32(this.next, val);
    this.next += 4;
  };

  Encoder.prototype.writeUint32 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    this.buffer.setUint32(this.next, val);
    this.next += 4;
  };

  Encoder.prototype.writeInt64 = function(val) {
    this.buffer.setInt64(this.next, val);
    this.next += 8;
  };

  Encoder.prototype.writeUint64 = function(val) {
    if (val < 0) {
      throw new Error(kErrorUnsigned);
    }
    this.buffer.setUint64(this.next, val);
    this.next += 8;
  };

  Encoder.prototype.writeFloat = function(val) {
    this.buffer.setFloat32(this.next, val);
    this.next += 4;
  };

  Encoder.prototype.writeDouble = function(val) {
    this.buffer.setFloat64(this.next, val);
    this.next += 8;
  };

  Encoder.prototype.encodePointer = function(pointer) {
    if (!pointer)
      return this.writeUint64(0);
    // TODO(abarth): To correctly encode a pointer, we need to know the real
    // base address of the array buffer.
    var offset = pointer - this.next;
    this.writeUint64(offset);
  };

  Encoder.prototype.createAndEncodeEncoder = function(size) {
    var pointer = this.buffer.alloc(align(size));
    this.encodePointer(pointer);
    return new Encoder(this.buffer, this.handles,
        this.associatedEndpointHandles, pointer);
  };

  Encoder.prototype.encodeHandle = function(handle) {
    if (handle) {
      this.handles.push(handle);
      this.writeUint32(this.handles.length - 1);
    } else {
      this.writeUint32(kEncodedInvalidHandleValue);
    }
  };

  Encoder.prototype.encodeAssociatedEndpointHandle = function(endpointHandle) {
    if (endpointHandle) {
      this.associatedEndpointHandles.push(endpointHandle);
      this.writeUint32(this.associatedEndpointHandles.length - 1);
    } else {
      this.writeUint32(kEncodedInvalidHandleValue);
    }
  };

  Encoder.prototype.encodeString = function(val) {
    var base = this.next + kArrayHeaderSize;
    var numberOfElements = internal.encodeUtf8String(
        val, new Uint8Array(this.buffer.arrayBuffer, base));
    var numberOfBytes = kArrayHeaderSize + numberOfElements;
    this.writeUint32(numberOfBytes);
    this.writeUint32(numberOfElements);
    this.next += numberOfElements;
  };

  Encoder.prototype.encodeArray =
      function(cls, val, numberOfElements, encodedSize) {
    if (numberOfElements === undefined)
      numberOfElements = val.length;
    if (encodedSize === undefined)
      encodedSize = kArrayHeaderSize + cls.encodedSize * numberOfElements;

    this.writeUint32(encodedSize);
    this.writeUint32(numberOfElements);

    if (cls === PackedBool) {
      var byte = 0;
      for (i = 0; i < numberOfElements; ++i) {
        if (val[i])
          byte |= (1 << i % 8);
        if (i % 8 === 7 || i == numberOfElements - 1) {
          Uint8.encode(this, byte);
          byte = 0;
        }
      }
    } else {
      for (var i = 0; i < numberOfElements; ++i)
        cls.encode(this, val[i]);
    }
  };

  Encoder.prototype.encodeStruct = function(cls, val) {
    return cls.encode(this, val);
  };

  Encoder.prototype.encodeStructPointer = function(cls, val) {
    if (val == null) {
      // Also handles undefined, since undefined == null.
      this.encodePointer(val);
      return;
    }
    var encoder = this.createAndEncodeEncoder(cls.encodedSize);
    cls.encode(encoder, val);
  };

  Encoder.prototype.encodeArrayPointer = function(cls, val) {
    if (val == null) {
      // Also handles undefined, since undefined == null.
      this.encodePointer(val);
      return;
    }

    var numberOfElements = val.length;
    if (!Number.isSafeInteger(numberOfElements) || numberOfElements < 0)
      throw new Error(kErrorArray);

    var encodedSize = kArrayHeaderSize + ((cls === PackedBool) ?
        Math.ceil(numberOfElements / 8) : cls.encodedSize * numberOfElements);
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeArray(cls, val, numberOfElements, encodedSize);
  };

  Encoder.prototype.encodeStringPointer = function(val) {
    if (val == null) {
      // Also handles undefined, since undefined == null.
      this.encodePointer(val);
      return;
    }
    // Only accepts string primivites, not String Objects like new String("foo")
    if (typeof(val) !== "string") {
      throw new Error(kErrorString);
    }
    var encodedSize = kArrayHeaderSize + internal.utf8Length(val);
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeString(val);
  };

  Encoder.prototype.encodeMap = function(keyClass, valueClass, val) {
    var keys = new Array(val.size);
    var values = new Array(val.size);
    var i = 0;
    val.forEach(function(value, key) {
      values[i] = value;
      keys[i++] = key;
    });
    this.writeUint32(kStructHeaderSize + kMapStructPayloadSize);
    this.writeUint32(0);  // version
    this.encodeArrayPointer(keyClass, keys);
    this.encodeArrayPointer(valueClass, values);
  }

  Encoder.prototype.encodeMapPointer = function(keyClass, valueClass, val) {
    if (val == null) {
      // Also handles undefined, since undefined == null.
      this.encodePointer(val);
      return;
    }
    if (!(val instanceof Map)) {
      throw new Error(kErrorMap);
    }
    var encodedSize = kStructHeaderSize + kMapStructPayloadSize;
    var encoder = this.createAndEncodeEncoder(encodedSize);
    encoder.encodeMap(keyClass, valueClass, val);
  };

  // Message ------------------------------------------------------------------

  var kMessageInterfaceIdOffset = kStructHeaderSize;
  var kMessageNameOffset = kMessageInterfaceIdOffset + 4;
  var kMessageFlagsOffset = kMessageNameOffset + 4;
  var kMessageRequestIDOffset = kMessageFlagsOffset + 8;
  var kMessagePayloadInterfaceIdsPointerOffset = kMessageV2HeaderSize - 8;

  var kMessageExpectsResponse = 1 << 0;
  var kMessageIsResponse      = 1 << 1;

  function Message(buffer, handles, associatedEndpointHandles) {
    if (associatedEndpointHandles === undefined) {
      associatedEndpointHandles = [];
    }

    this.buffer = buffer;
    this.handles = handles;
    this.associatedEndpointHandles = associatedEndpointHandles;
  }

  Message.prototype.getHeaderNumBytes = function() {
    return this.buffer.getUint32(kStructHeaderNumBytesOffset);
  };

  Message.prototype.getHeaderVersion = function() {
    return this.buffer.getUint32(kStructHeaderVersionOffset);
  };

  Message.prototype.getName = function() {
    return this.buffer.getUint32(kMessageNameOffset);
  };

  Message.prototype.getFlags = function() {
    return this.buffer.getUint32(kMessageFlagsOffset);
  };

  Message.prototype.getInterfaceId = function() {
    return this.buffer.getUint32(kMessageInterfaceIdOffset);
  };

  Message.prototype.getPayloadInterfaceIds = function() {
    if (this.getHeaderVersion() < 2) {
      return null;
    }

    var decoder = new Decoder(this.buffer, this.handles,
        this.associatedEndpointHandles,
        kMessagePayloadInterfaceIdsPointerOffset);
    var payloadInterfaceIds = decoder.decodeArrayPointer(Uint32);
    return payloadInterfaceIds;
  };

  Message.prototype.isResponse = function() {
    return (this.getFlags() & kMessageIsResponse) != 0;
  };

  Message.prototype.expectsResponse = function() {
    return (this.getFlags() & kMessageExpectsResponse) != 0;
  };

  Message.prototype.setRequestID = function(requestID) {
    // TODO(darin): Verify that space was reserved for this field!
    this.buffer.setUint64(kMessageRequestIDOffset, requestID);
  };

  Message.prototype.setInterfaceId = function(interfaceId) {
    this.buffer.setUint32(kMessageInterfaceIdOffset, interfaceId);
  };

  Message.prototype.setPayloadInterfaceIds_ = function(payloadInterfaceIds) {
    if (this.getHeaderVersion() < 2) {
      throw new Error(
          "Version of message does not support payload interface ids");
    }

    var decoder = new Decoder(this.buffer, this.handles,
        this.associatedEndpointHandles,
        kMessagePayloadInterfaceIdsPointerOffset);
    var payloadInterfaceIdsOffset = decoder.decodePointer();
    var encoder = new Encoder(this.buffer, this.handles,
        this.associatedEndpointHandles,
        payloadInterfaceIdsOffset);
    encoder.encodeArray(Uint32, payloadInterfaceIds);
  };

  Message.prototype.serializeAssociatedEndpointHandles = function(
      associatedGroupController) {
    if (this.associatedEndpointHandles.length > 0) {
      if (this.getHeaderVersion() < 2) {
        throw new Error(
            "Version of message does not support associated endpoint handles");
      }

      var data = [];
      for (var i = 0; i < this.associatedEndpointHandles.length; i++) {
        var handle = this.associatedEndpointHandles[i];
        data.push(associatedGroupController.associateInterface(handle));
      }
      this.associatedEndpointHandles = [];
      this.setPayloadInterfaceIds_(data);
    }
  };

  Message.prototype.deserializeAssociatedEndpointHandles = function(
      associatedGroupController) {
    if (this.getHeaderVersion() < 2) {
      return true;
    }

    this.associatedEndpointHandles = [];
    var ids = this.getPayloadInterfaceIds();

    var result = true;
    for (var i = 0; i < ids.length; i++) {
      var handle = associatedGroupController.createLocalEndpointHandle(ids[i]);
      if (internal.isValidInterfaceId(ids[i]) && !handle.isValid()) {
        // |ids[i]| itself is valid but handle creation failed. In that case,
        // mark deserialization as failed but continue to deserialize the
        // rest of handles.
        result = false;
      }
      this.associatedEndpointHandles.push(handle);
      ids[i] = internal.kInvalidInterfaceId;
    }

    this.setPayloadInterfaceIds_(ids);
    return result;
  };


  // MessageV0Builder ---------------------------------------------------------

  function MessageV0Builder(messageName, payloadSize) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageV0HeaderSize + payloadSize;
    this.buffer = new internal.Buffer(numberOfBytes);
    this.handles = [];
    var encoder = this.createEncoder(kMessageV0HeaderSize);
    encoder.writeUint32(kMessageV0HeaderSize);
    encoder.writeUint32(0);  // version.
    encoder.writeUint32(0);  // interface ID.
    encoder.writeUint32(messageName);
    encoder.writeUint32(0);  // flags.
    encoder.writeUint32(0);  // padding.
  }

  MessageV0Builder.prototype.createEncoder = function(size) {
    var pointer = this.buffer.alloc(size);
    return new Encoder(this.buffer, this.handles, [], pointer);
  };

  MessageV0Builder.prototype.encodeStruct = function(cls, val) {
    cls.encode(this.createEncoder(cls.encodedSize), val);
  };

  MessageV0Builder.prototype.finish = function() {
    // TODO(abarth): Rather than resizing the buffer at the end, we could
    // compute the size we need ahead of time, like we do in C++.
    this.buffer.trim();
    var message = new Message(this.buffer, this.handles);
    this.buffer = null;
    this.handles = null;
    this.encoder = null;
    return message;
  };

  // MessageV1Builder -----------------------------------------------

  function MessageV1Builder(messageName, payloadSize, flags,
                                       requestID) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageV1HeaderSize + payloadSize;
    this.buffer = new internal.Buffer(numberOfBytes);
    this.handles = [];
    var encoder = this.createEncoder(kMessageV1HeaderSize);
    encoder.writeUint32(kMessageV1HeaderSize);
    encoder.writeUint32(1);  // version.
    encoder.writeUint32(0);  // interface ID.
    encoder.writeUint32(messageName);
    encoder.writeUint32(flags);
    encoder.writeUint32(0);  // padding.
    encoder.writeUint64(requestID);
  }

  MessageV1Builder.prototype =
      Object.create(MessageV0Builder.prototype);

  MessageV1Builder.prototype.constructor =
      MessageV1Builder;

  // MessageV2 -----------------------------------------------

  function MessageV2Builder(messageName, payloadSize, flags, requestID) {
    // Currently, we don't compute the payload size correctly ahead of time.
    // Instead, we resize the buffer at the end.
    var numberOfBytes = kMessageV2HeaderSize + payloadSize;
    this.buffer = new internal.Buffer(numberOfBytes);
    this.handles = [];

    this.payload = null;
    this.associatedEndpointHandles = [];

    this.encoder = this.createEncoder(kMessageV2HeaderSize);
    this.encoder.writeUint32(kMessageV2HeaderSize);
    this.encoder.writeUint32(2);  // version.
    // Gets set to an appropriate interfaceId for the endpoint by the Router.
    this.encoder.writeUint32(0);  // interface ID.
    this.encoder.writeUint32(messageName);
    this.encoder.writeUint32(flags);
    this.encoder.writeUint32(0);  // padding.
    this.encoder.writeUint64(requestID);
  }

  MessageV2Builder.prototype.createEncoder = function(size) {
    var pointer = this.buffer.alloc(size);
    return new Encoder(this.buffer, this.handles,
        this.associatedEndpointHandles, pointer);
  };

  MessageV2Builder.prototype.setPayload = function(cls, val) {
    this.payload = {cls: cls, val: val};
  };

  MessageV2Builder.prototype.finish = function() {
    if (!this.payload) {
      throw new Error("Payload needs to be set before calling finish");
    }

    this.encoder.encodeStructPointer(this.payload.cls, this.payload.val);
    this.encoder.encodeArrayPointer(Uint32,
        new Array(this.associatedEndpointHandles.length));

    this.buffer.trim();
    var message = new Message(this.buffer, this.handles,
        this.associatedEndpointHandles);
    this.buffer = null;
    this.handles = null;
    this.encoder = null;
    this.payload = null;
    this.associatedEndpointHandles = null;

    return message;
  };

  // MessageReader ------------------------------------------------------------

  function MessageReader(message) {
    this.decoder = new Decoder(message.buffer, message.handles,
        message.associatedEndpointHandles, 0);
    var messageHeaderSize = this.decoder.readUint32();
    this.payloadSize = message.buffer.byteLength - messageHeaderSize;
    var version = this.decoder.readUint32();
    var interface_id = this.decoder.readUint32();
    this.messageName = this.decoder.readUint32();
    this.flags = this.decoder.readUint32();
    // Skip the padding.
    this.decoder.skip(4);
    if (version >= 1)
      this.requestID = this.decoder.readUint64();
    this.decoder.skip(messageHeaderSize - this.decoder.next);
  }

  MessageReader.prototype.decodeStruct = function(cls) {
    return cls.decode(this.decoder);
  };

  // Built-in types -----------------------------------------------------------

  // This type is only used with ArrayOf(PackedBool).
  function PackedBool() {
  }

  function Int8() {
  }

  Int8.encodedSize = 1;

  Int8.decode = function(decoder) {
    return decoder.readInt8();
  };

  Int8.encode = function(encoder, val) {
    encoder.writeInt8(val);
  };

  Uint8.encode = function(encoder, val) {
    encoder.writeUint8(val);
  };

  function Uint8() {
  }

  Uint8.encodedSize = 1;

  Uint8.decode = function(decoder) {
    return decoder.readUint8();
  };

  Uint8.encode = function(encoder, val) {
    encoder.writeUint8(val);
  };

  function Int16() {
  }

  Int16.encodedSize = 2;

  Int16.decode = function(decoder) {
    return decoder.readInt16();
  };

  Int16.encode = function(encoder, val) {
    encoder.writeInt16(val);
  };

  function Uint16() {
  }

  Uint16.encodedSize = 2;

  Uint16.decode = function(decoder) {
    return decoder.readUint16();
  };

  Uint16.encode = function(encoder, val) {
    encoder.writeUint16(val);
  };

  function Int32() {
  }

  Int32.encodedSize = 4;

  Int32.decode = function(decoder) {
    return decoder.readInt32();
  };

  Int32.encode = function(encoder, val) {
    encoder.writeInt32(val);
  };

  function Uint32() {
  }

  Uint32.encodedSize = 4;

  Uint32.decode = function(decoder) {
    return decoder.readUint32();
  };

  Uint32.encode = function(encoder, val) {
    encoder.writeUint32(val);
  };

  function Int64() {
  }

  Int64.encodedSize = 8;

  Int64.decode = function(decoder) {
    return decoder.readInt64();
  };

  Int64.encode = function(encoder, val) {
    encoder.writeInt64(val);
  };

  function Uint64() {
  }

  Uint64.encodedSize = 8;

  Uint64.decode = function(decoder) {
    return decoder.readUint64();
  };

  Uint64.encode = function(encoder, val) {
    encoder.writeUint64(val);
  };

  function String() {
  };

  String.encodedSize = 8;

  String.decode = function(decoder) {
    return decoder.decodeStringPointer();
  };

  String.encode = function(encoder, val) {
    encoder.encodeStringPointer(val);
  };

  function NullableString() {
  }

  NullableString.encodedSize = String.encodedSize;

  NullableString.decode = String.decode;

  NullableString.encode = String.encode;

  function Float() {
  }

  Float.encodedSize = 4;

  Float.decode = function(decoder) {
    return decoder.readFloat();
  };

  Float.encode = function(encoder, val) {
    encoder.writeFloat(val);
  };

  function Double() {
  }

  Double.encodedSize = 8;

  Double.decode = function(decoder) {
    return decoder.readDouble();
  };

  Double.encode = function(encoder, val) {
    encoder.writeDouble(val);
  };

  function Enum(cls) {
    this.cls = cls;
  }

  Enum.prototype.encodedSize = 4;

  Enum.prototype.decode = function(decoder) {
    return decoder.readInt32();
  };

  Enum.prototype.encode = function(encoder, val) {
    encoder.writeInt32(val);
  };

  function PointerTo(cls) {
    this.cls = cls;
  }

  PointerTo.prototype.encodedSize = 8;

  PointerTo.prototype.decode = function(decoder) {
    var pointer = decoder.decodePointer();
    if (!pointer) {
      return null;
    }
    return this.cls.decode(decoder.decodeAndCreateDecoder(pointer));
  };

  PointerTo.prototype.encode = function(encoder, val) {
    if (!val) {
      encoder.encodePointer(val);
      return;
    }
    var objectEncoder = encoder.createAndEncodeEncoder(this.cls.encodedSize);
    this.cls.encode(objectEncoder, val);
  };

  function NullablePointerTo(cls) {
    PointerTo.call(this, cls);
  }

  NullablePointerTo.prototype = Object.create(PointerTo.prototype);

  function ArrayOf(cls, length) {
    this.cls = cls;
    this.length = length || 0;
  }

  ArrayOf.prototype.encodedSize = 8;

  ArrayOf.prototype.dimensions = function() {
    return [this.length].concat(
      (this.cls instanceof ArrayOf) ? this.cls.dimensions() : []);
  }

  ArrayOf.prototype.decode = function(decoder) {
    return decoder.decodeArrayPointer(this.cls);
  };

  ArrayOf.prototype.encode = function(encoder, val) {
    encoder.encodeArrayPointer(this.cls, val);
  };

  function NullableArrayOf(cls) {
    ArrayOf.call(this, cls);
  }

  NullableArrayOf.prototype = Object.create(ArrayOf.prototype);

  function Handle() {
  }

  Handle.encodedSize = 4;

  Handle.decode = function(decoder) {
    return decoder.decodeHandle();
  };

  Handle.encode = function(encoder, val) {
    encoder.encodeHandle(val);
  };

  function NullableHandle() {
  }

  NullableHandle.encodedSize = Handle.encodedSize;

  NullableHandle.decode = Handle.decode;

  NullableHandle.encode = Handle.encode;

  function Interface(cls) {
    this.cls = cls;
  }

  Interface.prototype.encodedSize = 8;

  Interface.prototype.decode = function(decoder) {
    var interfacePtrInfo = new mojo.InterfacePtrInfo(
        decoder.decodeHandle(), decoder.readUint32());
    var interfacePtr = new this.cls();
    interfacePtr.ptr.bind(interfacePtrInfo);
    return interfacePtr;
  };

  Interface.prototype.encode = function(encoder, val) {
    var interfacePtrInfo =
        val ? val.ptr.passInterface() : new mojo.InterfacePtrInfo(null, 0);
    encoder.encodeHandle(interfacePtrInfo.handle);
    encoder.writeUint32(interfacePtrInfo.version);
  };

  function NullableInterface(cls) {
    Interface.call(this, cls);
  }

  NullableInterface.prototype = Object.create(Interface.prototype);

  function AssociatedInterfacePtrInfo() {
  }

  AssociatedInterfacePtrInfo.prototype.encodedSize = 8;

  AssociatedInterfacePtrInfo.decode = function(decoder) {
    return new mojo.AssociatedInterfacePtrInfo(
      decoder.decodeAssociatedEndpointHandle(), decoder.readUint32());
  };

  AssociatedInterfacePtrInfo.encode = function(encoder, val) {
    var associatedinterfacePtrInfo =
        val ? val : new mojo.AssociatedInterfacePtrInfo(null, 0);
    encoder.encodeAssociatedEndpointHandle(
        associatedinterfacePtrInfo.interfaceEndpointHandle);
    encoder.writeUint32(associatedinterfacePtrInfo.version);
  };

  function NullableAssociatedInterfacePtrInfo() {
  }

  NullableAssociatedInterfacePtrInfo.encodedSize =
      AssociatedInterfacePtrInfo.encodedSize;

  NullableAssociatedInterfacePtrInfo.decode =
      AssociatedInterfacePtrInfo.decode;

  NullableAssociatedInterfacePtrInfo.encode =
      AssociatedInterfacePtrInfo.encode;

  function InterfaceRequest() {
  }

  InterfaceRequest.encodedSize = 4;

  InterfaceRequest.decode = function(decoder) {
    return new mojo.InterfaceRequest(decoder.decodeHandle());
  };

  InterfaceRequest.encode = function(encoder, val) {
    encoder.encodeHandle(val ? val.handle : null);
  };

  function NullableInterfaceRequest() {
  }

  NullableInterfaceRequest.encodedSize = InterfaceRequest.encodedSize;

  NullableInterfaceRequest.decode = InterfaceRequest.decode;

  NullableInterfaceRequest.encode = InterfaceRequest.encode;

  function AssociatedInterfaceRequest() {
  }

  AssociatedInterfaceRequest.decode = function(decoder) {
    var handle = decoder.decodeAssociatedEndpointHandle();
    return new mojo.AssociatedInterfaceRequest(handle);
  };

  AssociatedInterfaceRequest.encode = function(encoder, val) {
    encoder.encodeAssociatedEndpointHandle(
        val ? val.interfaceEndpointHandle : null);
  };

  AssociatedInterfaceRequest.encodedSize = 4;

  function NullableAssociatedInterfaceRequest() {
  }

  NullableAssociatedInterfaceRequest.encodedSize =
      AssociatedInterfaceRequest.encodedSize;

  NullableAssociatedInterfaceRequest.decode =
      AssociatedInterfaceRequest.decode;

  NullableAssociatedInterfaceRequest.encode =
      AssociatedInterfaceRequest.encode;

  function MapOf(keyClass, valueClass) {
    this.keyClass = keyClass;
    this.valueClass = valueClass;
  }

  MapOf.prototype.encodedSize = 8;

  MapOf.prototype.decode = function(decoder) {
    return decoder.decodeMapPointer(this.keyClass, this.valueClass);
  };

  MapOf.prototype.encode = function(encoder, val) {
    encoder.encodeMapPointer(this.keyClass, this.valueClass, val);
  };

  function NullableMapOf(keyClass, valueClass) {
    MapOf.call(this, keyClass, valueClass);
  }

  NullableMapOf.prototype = Object.create(MapOf.prototype);

  internal.align = align;
  internal.isAligned = isAligned;
  internal.Message = Message;
  internal.MessageV0Builder = MessageV0Builder;
  internal.MessageV1Builder = MessageV1Builder;
  internal.MessageV2Builder = MessageV2Builder;
  internal.MessageReader = MessageReader;
  internal.kArrayHeaderSize = kArrayHeaderSize;
  internal.kMapStructPayloadSize = kMapStructPayloadSize;
  internal.kStructHeaderSize = kStructHeaderSize;
  internal.kEncodedInvalidHandleValue = kEncodedInvalidHandleValue;
  internal.kMessageV0HeaderSize = kMessageV0HeaderSize;
  internal.kMessageV1HeaderSize = kMessageV1HeaderSize;
  internal.kMessageV2HeaderSize = kMessageV2HeaderSize;
  internal.kMessagePayloadInterfaceIdsPointerOffset =
      kMessagePayloadInterfaceIdsPointerOffset;
  internal.kMessageExpectsResponse = kMessageExpectsResponse;
  internal.kMessageIsResponse = kMessageIsResponse;
  internal.Int8 = Int8;
  internal.Uint8 = Uint8;
  internal.Int16 = Int16;
  internal.Uint16 = Uint16;
  internal.Int32 = Int32;
  internal.Uint32 = Uint32;
  internal.Int64 = Int64;
  internal.Uint64 = Uint64;
  internal.Float = Float;
  internal.Double = Double;
  internal.String = String;
  internal.Enum = Enum;
  internal.NullableString = NullableString;
  internal.PointerTo = PointerTo;
  internal.NullablePointerTo = NullablePointerTo;
  internal.ArrayOf = ArrayOf;
  internal.NullableArrayOf = NullableArrayOf;
  internal.PackedBool = PackedBool;
  internal.Handle = Handle;
  internal.NullableHandle = NullableHandle;
  internal.Interface = Interface;
  internal.NullableInterface = NullableInterface;
  internal.InterfaceRequest = InterfaceRequest;
  internal.NullableInterfaceRequest = NullableInterfaceRequest;
  internal.AssociatedInterfacePtrInfo = AssociatedInterfacePtrInfo;
  internal.NullableAssociatedInterfacePtrInfo =
      NullableAssociatedInterfacePtrInfo;
  internal.AssociatedInterfaceRequest = AssociatedInterfaceRequest;
  internal.NullableAssociatedInterfaceRequest =
      NullableAssociatedInterfaceRequest;
  internal.MapOf = MapOf;
  internal.NullableMapOf = NullableMapOf;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function Connector(handle) {
    if (!(handle instanceof MojoHandle))
      throw new Error("Connector: not a handle " + handle);
    this.handle_ = handle;
    this.dropWrites_ = false;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWatcher_ = null;
    this.errorHandler_ = null;
    this.paused_ = false;

    this.waitToReadMore();
  }

  Connector.prototype.close = function() {
    this.cancelWait();
    if (this.handle_ != null) {
      this.handle_.close();
      this.handle_ = null;
    }
  };

  Connector.prototype.pauseIncomingMethodCallProcessing = function() {
    if (this.paused_) {
      return;
    }
    this.paused_= true;
    this.cancelWait();
  };

  Connector.prototype.resumeIncomingMethodCallProcessing = function() {
    if (!this.paused_) {
      return;
    }
    this.paused_= false;
    this.waitToReadMore();
  };

  Connector.prototype.accept = function(message) {
    if (this.error_)
      return false;

    if (this.dropWrites_)
      return true;

    var result = this.handle_.writeMessage(
        new Uint8Array(message.buffer.arrayBuffer), message.handles);
    switch (result) {
      case Mojo.RESULT_OK:
        // The handles were successfully transferred, so we don't own them
        // anymore.
        message.handles = [];
        break;
      case Mojo.RESULT_FAILED_PRECONDITION:
        // There's no point in continuing to write to this pipe since the other
        // end is gone. Avoid writing any future messages. Hide write failures
        // from the caller since we'd like them to continue consuming any
        // backlog of incoming messages before regarding the message pipe as
        // closed.
        this.dropWrites_ = true;
        break;
      default:
        // This particular write was rejected, presumably because of bad input.
        // The pipe is not necessarily in a bad state.
        return false;
    }
    return true;
  };

  Connector.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  Connector.prototype.setErrorHandler = function(handler) {
    this.errorHandler_ = handler;
  };

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      if (this.paused_) {
        return;
      }

      var read = this.handle_.readMessage();
      if (this.handle_ == null) // The connector has been closed.
        return;
      if (read.result == Mojo.RESULT_SHOULD_WAIT)
        return;
      if (read.result != Mojo.RESULT_OK) {
        this.handleError(read.result !== Mojo.RESULT_FAILED_PRECONDITION,
            false);
        return;
      }
      var messageBuffer = new internal.Buffer(read.buffer);
      var message = new internal.Message(messageBuffer, read.handles);
      var receiverResult = this.incomingReceiver_ &&
          this.incomingReceiver_.accept(message);

      // Handle invalid incoming message.
      if (!internal.isTestingMode() && !receiverResult) {
        // TODO(yzshen): Consider notifying the embedder.
        this.handleError(true, false);
      }
    }
  };

  Connector.prototype.cancelWait = function() {
    if (this.readWatcher_) {
      this.readWatcher_.cancel();
      this.readWatcher_ = null;
    }
  };

  Connector.prototype.waitToReadMore = function() {
    if (this.handle_) {
      this.readWatcher_ = this.handle_.watch({readable: true},
                                             this.readMore_.bind(this));
    }
  };

  Connector.prototype.handleError = function(forcePipeReset,
                                             forceAsyncHandler) {
    if (this.error_ || this.handle_ === null) {
      return;
    }

    if (this.paused_) {
      // Enforce calling the error handler asynchronously if the user has
      // paused receiving messages. We need to wait until the user starts
      // receiving messages again.
      forceAsyncHandler = true;
    }

    if (!forcePipeReset && forceAsyncHandler) {
      forcePipeReset = true;
    }

    this.cancelWait();
    if (forcePipeReset) {
      this.handle_.close();
      var dummyPipe = Mojo.createMessagePipe();
      this.handle_ = dummyPipe.handle0;
    }

    if (forceAsyncHandler) {
      if (!this.paused_) {
        this.waitToReadMore();
      }
    } else {
      this.error_ = true;
      if (this.errorHandler_) {
        this.errorHandler_.onError();
      }
    }
  };

  internal.Connector = Connector;
})();
// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  // Constants ----------------------------------------------------------------
  var kInterfaceIdNamespaceMask = 0x80000000;
  var kMasterInterfaceId = 0x00000000;
  var kInvalidInterfaceId = 0xFFFFFFFF;

  // ---------------------------------------------------------------------------

  function InterfacePtrInfo(handle, version) {
    this.handle = handle;
    this.version = version;
  }

  InterfacePtrInfo.prototype.isValid = function() {
    return this.handle instanceof MojoHandle;
  };

  InterfacePtrInfo.prototype.close = function() {
    if (!this.isValid())
      return;

    this.handle.close();
    this.handle = null;
    this.version = 0;
  };

  function AssociatedInterfacePtrInfo(interfaceEndpointHandle, version) {
    this.interfaceEndpointHandle = interfaceEndpointHandle;
    this.version = version;
  }

  AssociatedInterfacePtrInfo.prototype.isValid = function() {
    return this.interfaceEndpointHandle.isValid();
  };

  // ---------------------------------------------------------------------------

  function InterfaceRequest(handle) {
    this.handle = handle;
  }

  InterfaceRequest.prototype.isValid = function() {
    return this.handle instanceof MojoHandle;
  };

  InterfaceRequest.prototype.close = function() {
    if (!this.isValid())
      return;

    this.handle.close();
    this.handle = null;
  };

  function AssociatedInterfaceRequest(interfaceEndpointHandle) {
    this.interfaceEndpointHandle = interfaceEndpointHandle;
  }

  AssociatedInterfaceRequest.prototype.isValid = function() {
    return this.interfaceEndpointHandle.isValid();
  };

  AssociatedInterfaceRequest.prototype.resetWithReason = function(reason) {
    this.interfaceEndpointHandle.reset(reason);
  };

  function isMasterInterfaceId(interfaceId) {
    return interfaceId === kMasterInterfaceId;
  }

  function isValidInterfaceId(interfaceId) {
    return interfaceId !== kInvalidInterfaceId;
  }

  mojo.InterfacePtrInfo = InterfacePtrInfo;
  mojo.InterfaceRequest = InterfaceRequest;
  mojo.AssociatedInterfacePtrInfo = AssociatedInterfacePtrInfo;
  mojo.AssociatedInterfaceRequest = AssociatedInterfaceRequest;
  internal.isMasterInterfaceId = isMasterInterfaceId;
  internal.isValidInterfaceId = isValidInterfaceId;
  internal.kInvalidInterfaceId = kInvalidInterfaceId;
  internal.kMasterInterfaceId = kMasterInterfaceId;
  internal.kInterfaceIdNamespaceMask = kInterfaceIdNamespaceMask;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function validateControlRequestWithResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsRequestExpectingResponse();
    if (error !== internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interfaceControl2.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = mojo.interfaceControl2.RunMessageParams.validate(messageValidator,
        message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function validateControlRequestWithoutResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsRequestWithoutResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interfaceControl2.kRunOrClosePipeMessageId) {
      throw new Error("Control message name is not kRunOrClosePipeMessageId");
    }

    // Validate payload.
    error = mojo.interfaceControl2.RunOrClosePipeMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function runOrClosePipe(message, interfaceVersion) {
    var reader = new internal.MessageReader(message);
    var runOrClosePipeMessageParams = reader.decodeStruct(
        mojo.interfaceControl2.RunOrClosePipeMessageParams);
    return interfaceVersion >=
        runOrClosePipeMessageParams.input.requireVersion.version;
  }

  function run(message, responder, interfaceVersion) {
    var reader = new internal.MessageReader(message);
    var runMessageParams =
        reader.decodeStruct(mojo.interfaceControl2.RunMessageParams);
    var runOutput = null;

    if (runMessageParams.input.queryVersion) {
      runOutput = new mojo.interfaceControl2.RunOutput();
      runOutput.queryVersionResult = new
          mojo.interfaceControl2.QueryVersionResult(
              {'version': interfaceVersion});
    }

    var runResponseMessageParams = new
        mojo.interfaceControl2.RunResponseMessageParams();
    runResponseMessageParams.output = runOutput;

    var messageName = mojo.interfaceControl2.kRunMessageId;
    var payloadSize =
        mojo.interfaceControl2.RunResponseMessageParams.encodedSize;
    var requestID = reader.requestID;
    var builder = new internal.MessageV1Builder(messageName,
        payloadSize, internal.kMessageIsResponse, requestID);
    builder.encodeStruct(mojo.interfaceControl2.RunResponseMessageParams,
                         runResponseMessageParams);
    responder.accept(builder.finish());
    return true;
  }

  function isInterfaceControlMessage(message) {
    return message.getName() == mojo.interfaceControl2.kRunMessageId ||
           message.getName() == mojo.interfaceControl2.kRunOrClosePipeMessageId;
  }

  function ControlMessageHandler(interfaceVersion) {
    this.interfaceVersion_ = interfaceVersion;
  }

  ControlMessageHandler.prototype.accept = function(message) {
    validateControlRequestWithoutResponse(message);
    return runOrClosePipe(message, this.interfaceVersion_);
  };

  ControlMessageHandler.prototype.acceptWithResponder = function(message,
      responder) {
    validateControlRequestWithResponse(message);
    return run(message, responder, this.interfaceVersion_);
  };

  internal.ControlMessageHandler = ControlMessageHandler;
  internal.isInterfaceControlMessage = isInterfaceControlMessage;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function constructRunOrClosePipeMessage(runOrClosePipeInput) {
    var runOrClosePipeMessageParams = new
        mojo.interfaceControl2.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = runOrClosePipeInput;

    var messageName = mojo.interfaceControl2.kRunOrClosePipeMessageId;
    var payloadSize =
        mojo.interfaceControl2.RunOrClosePipeMessageParams.encodedSize;
    var builder = new internal.MessageV0Builder(messageName, payloadSize);
    builder.encodeStruct(mojo.interfaceControl2.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    return message;
  }

  function validateControlResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.interfaceControl2.kRunMessageId) {
      throw new Error("Control message name is not kRunMessageId");
    }

    // Validate payload.
    error = mojo.interfaceControl2.RunResponseMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function acceptRunResponse(message) {
    validateControlResponse(message);

    var reader = new internal.MessageReader(message);
    var runResponseMessageParams = reader.decodeStruct(
        mojo.interfaceControl2.RunResponseMessageParams);

    return Promise.resolve(runResponseMessageParams);
  }

 /**
  * Sends the given run message through the receiver.
  * Accepts the response message from the receiver and decodes the message
  * struct to RunResponseMessageParams.
  *
  * @param  {Router} receiver.
  * @param  {RunMessageParams} runMessageParams to be sent via a message.
  * @return {Promise} that resolves to a RunResponseMessageParams.
  */
  function sendRunMessage(receiver, runMessageParams) {
    var messageName = mojo.interfaceControl2.kRunMessageId;
    var payloadSize = mojo.interfaceControl2.RunMessageParams.encodedSize;
    // |requestID| is set to 0, but is later properly set by Router.
    var builder = new internal.MessageV1Builder(messageName,
        payloadSize, internal.kMessageExpectsResponse, 0);
    builder.encodeStruct(mojo.interfaceControl2.RunMessageParams,
                         runMessageParams);
    var message = builder.finish();

    return receiver.acceptAndExpectResponse(message).then(acceptRunResponse);
  }

  function ControlMessageProxy(receiver) {
    this.receiver_ = receiver;
  }

  ControlMessageProxy.prototype.queryVersion = function() {
    var runMessageParams = new mojo.interfaceControl2.RunMessageParams();
    runMessageParams.input = new mojo.interfaceControl2.RunInput();
    runMessageParams.input.queryVersion =
        new mojo.interfaceControl2.QueryVersion();

    return sendRunMessage(this.receiver_, runMessageParams).then(function(
        runResponseMessageParams) {
      return runResponseMessageParams.output.queryVersionResult.version;
    });
  };

  ControlMessageProxy.prototype.requireVersion = function(version) {
    var runOrClosePipeInput = new mojo.interfaceControl2.RunOrClosePipeInput();
    runOrClosePipeInput.requireVersion =
        new mojo.interfaceControl2.RequireVersion({'version': version});
    var message = constructRunOrClosePipeMessage(runOrClosePipeInput);
    this.receiver_.accept(message);
  };

  internal.ControlMessageProxy = ControlMessageProxy;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function InterfaceEndpointClient(interfaceEndpointHandle, receiver,
      interfaceVersion) {
    this.controller_ = null;
    this.encounteredError_ = false;
    this.handle_ = interfaceEndpointHandle;
    this.incomingReceiver_ = receiver;

    if (interfaceVersion !== undefined) {
      this.controlMessageHandler_ = new internal.ControlMessageHandler(
          interfaceVersion);
    } else {
      this.controlMessageProxy_ = new internal.ControlMessageProxy(this);
    }

    this.nextRequestID_ = 0;
    this.completers_ = new Map();
    this.payloadValidators_ = [];
    this.connectionErrorHandler_ = null;

    if (interfaceEndpointHandle.pendingAssociation()) {
      interfaceEndpointHandle.setAssociationEventHandler(
          this.onAssociationEvent.bind(this));
    } else {
      this.initControllerIfNecessary_();
    }
  }

  InterfaceEndpointClient.prototype.initControllerIfNecessary_ = function() {
    if (this.controller_ || this.handle_.pendingAssociation()) {
      return;
    }

    this.controller_ = this.handle_.groupController().attachEndpointClient(
        this.handle_, this);
  };

  InterfaceEndpointClient.prototype.onAssociationEvent = function(
      associationEvent) {
    if (associationEvent === internal.AssociationEvent.ASSOCIATED) {
      this.initControllerIfNecessary_();
    } else if (associationEvent ===
          internal.AssociationEvent.PEER_CLOSED_BEFORE_ASSOCIATION) {
      setTimeout(this.notifyError.bind(this, this.handle_.disconnectReason()),
                 0);
    }
  };

  InterfaceEndpointClient.prototype.passHandle = function() {
    if (!this.handle_.isValid()) {
      return new internal.InterfaceEndpointHandle();
    }

    // Used to clear the previously set callback.
    this.handle_.setAssociationEventHandler(undefined);

    if (this.controller_) {
      this.controller_ = null;
      this.handle_.groupController().detachEndpointClient(this.handle_);
    }
    var handle = this.handle_;
    this.handle_ = null;
    return handle;
  };

  InterfaceEndpointClient.prototype.close = function(reason) {
    var handle = this.passHandle();
    handle.reset(reason);
  };

  InterfaceEndpointClient.prototype.accept = function(message) {
    if (message.associatedEndpointHandles.length > 0) {
      message.serializeAssociatedEndpointHandles(
          this.handle_.groupController());
    }

    if (this.encounteredError_) {
      return false;
    }

    this.initControllerIfNecessary_();
    return this.controller_.sendMessage(message);
  };

  InterfaceEndpointClient.prototype.acceptAndExpectResponse = function(
      message) {
    if (message.associatedEndpointHandles.length > 0) {
      message.serializeAssociatedEndpointHandles(
          this.handle_.groupController());
    }

    if (this.encounteredError_) {
      return Promise.reject();
    }

    this.initControllerIfNecessary_();

    // Reserve 0 in case we want it to convey special meaning in the future.
    var requestID = this.nextRequestID_++;
    if (requestID === 0)
      requestID = this.nextRequestID_++;

    message.setRequestID(requestID);
    var result = this.controller_.sendMessage(message);
    if (!result)
      return Promise.reject(Error("Connection error"));

    var completer = {};
    this.completers_.set(requestID, completer);
    return new Promise(function(resolve, reject) {
      completer.resolve = resolve;
      completer.reject = reject;
    });
  };

  InterfaceEndpointClient.prototype.setPayloadValidators = function(
      payloadValidators) {
    this.payloadValidators_ = payloadValidators;
  };

  InterfaceEndpointClient.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  InterfaceEndpointClient.prototype.setConnectionErrorHandler = function(
      handler) {
    this.connectionErrorHandler_ = handler;
  };

  InterfaceEndpointClient.prototype.handleIncomingMessage = function(message,
      messageValidator) {
    var noError = internal.validationError.NONE;
    var err = noError;
    for (var i = 0; err === noError && i < this.payloadValidators_.length; ++i)
      err = this.payloadValidators_[i](messageValidator);

    if (err == noError) {
      return this.handleValidIncomingMessage_(message);
    } else {
      internal.reportValidationError(err);
      return false;
    }
  };

  InterfaceEndpointClient.prototype.handleValidIncomingMessage_ = function(
      message) {
    if (internal.isTestingMode()) {
      return true;
    }

    if (this.encounteredError_) {
      return false;
    }

    var ok = false;

    if (message.expectsResponse()) {
      if (internal.isInterfaceControlMessage(message) &&
          this.controlMessageHandler_) {
        ok = this.controlMessageHandler_.acceptWithResponder(message, this);
      } else if (this.incomingReceiver_) {
        ok = this.incomingReceiver_.acceptWithResponder(message, this);
      }
    } else if (message.isResponse()) {
      var reader = new internal.MessageReader(message);
      var requestID = reader.requestID;
      var completer = this.completers_.get(requestID);
      if (completer) {
        this.completers_.delete(requestID);
        completer.resolve(message);
        ok = true;
      } else {
        console.log("Unexpected response with request ID: " + requestID);
      }
    } else {
      if (internal.isInterfaceControlMessage(message) &&
          this.controlMessageHandler_) {
        ok = this.controlMessageHandler_.accept(message);
      } else if (this.incomingReceiver_) {
        ok = this.incomingReceiver_.accept(message);
      }
    }
    return ok;
  };

  InterfaceEndpointClient.prototype.notifyError = function(reason) {
    if (this.encounteredError_) {
      return;
    }
    this.encounteredError_ = true;

    this.completers_.forEach(function(value) {
      value.reject();
    });
    this.completers_.clear();  // Drop any responders.

    if (this.connectionErrorHandler_) {
      this.connectionErrorHandler_(reason);
    }
  };

  InterfaceEndpointClient.prototype.queryVersion = function() {
    return this.controlMessageProxy_.queryVersion();
  };

  InterfaceEndpointClient.prototype.requireVersion = function(version) {
    this.controlMessageProxy_.requireVersion(version);
  };

  InterfaceEndpointClient.prototype.getEncounteredError = function() {
    return this.encounteredError_;
  };

  internal.InterfaceEndpointClient = InterfaceEndpointClient;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  var AssociationEvent = {
    // The interface has been associated with a message pipe.
    ASSOCIATED: 'associated',
    // The peer of this object has been closed before association.
    PEER_CLOSED_BEFORE_ASSOCIATION: 'peer_closed_before_association'
  };

  function State(interfaceId, associatedGroupController) {
    if (interfaceId === undefined) {
      interfaceId = internal.kInvalidInterfaceId;
    }

    this.interfaceId = interfaceId;
    this.associatedGroupController = associatedGroupController;
    this.pendingAssociation = false;
    this.disconnectReason = null;
    this.peerState_ = null;
    this.associationEventHandler_ = null;
  }

  State.prototype.initPendingState = function(peer) {
    this.pendingAssociation = true;
    this.peerState_ = peer;
  };

  State.prototype.isValid = function() {
    return this.pendingAssociation ||
        internal.isValidInterfaceId(this.interfaceId);
  };

  State.prototype.close = function(disconnectReason) {
    var cachedGroupController;
    var cachedPeerState;
    var cachedId = internal.kInvalidInterfaceId;

    if (!this.pendingAssociation) {
      if (internal.isValidInterfaceId(this.interfaceId)) {
        cachedGroupController = this.associatedGroupController;
        this.associatedGroupController = null;
        cachedId = this.interfaceId;
        this.interfaceId = internal.kInvalidInterfaceId;
      }
    } else {
      this.pendingAssociation = false;
      cachedPeerState = this.peerState_;
      this.peerState_ = null;
    }

    if (cachedGroupController) {
      cachedGroupController.closeEndpointHandle(cachedId,
          disconnectReason);
    } else if (cachedPeerState) {
      cachedPeerState.onPeerClosedBeforeAssociation(disconnectReason);
    }
  };

  State.prototype.runAssociationEventHandler = function(associationEvent) {
    if (this.associationEventHandler_) {
      var handler = this.associationEventHandler_;
      this.associationEventHandler_ = null;
      handler(associationEvent);
    }
  };

  State.prototype.setAssociationEventHandler = function(handler) {
    if (!this.pendingAssociation &&
        !internal.isValidInterfaceId(this.interfaceId)) {
      return;
    }

    if (!handler) {
      this.associationEventHandler_ = null;
      return;
    }

    this.associationEventHandler_ = handler;
    if (!this.pendingAssociation) {
      setTimeout(this.runAssociationEventHandler.bind(this,
          AssociationEvent.ASSOCIATED), 0);
    } else if (!this.peerState_) {
      setTimeout(this.runAssociationEventHandler.bind(this,
          AssociationEvent.PEER_CLOSED_BEFORE_ASSOCIATION), 0);
    }
  };

  State.prototype.notifyAssociation = function(interfaceId,
                                               peerGroupController) {
    var cachedPeerState = this.peerState_;
    this.peerState_ = null;

    this.pendingAssociation = false;

    if (cachedPeerState) {
      cachedPeerState.onAssociated(interfaceId, peerGroupController);
      return true;
    }
    return false;
  };

  State.prototype.onAssociated = function(interfaceId,
      associatedGroupController) {
    if (!this.pendingAssociation) {
      return;
    }

    this.pendingAssociation = false;
    this.peerState_ = null;
    this.interfaceId = interfaceId;
    this.associatedGroupController = associatedGroupController;
    this.runAssociationEventHandler(AssociationEvent.ASSOCIATED);
  };

  State.prototype.onPeerClosedBeforeAssociation = function(disconnectReason) {
    if (!this.pendingAssociation) {
      return;
    }

    this.peerState_ = null;
    this.disconnectReason = disconnectReason;

    this.runAssociationEventHandler(
        AssociationEvent.PEER_CLOSED_BEFORE_ASSOCIATION);
  };

  function createPairPendingAssociation() {
    var handle0 = new InterfaceEndpointHandle();
    var handle1 = new InterfaceEndpointHandle();
    handle0.state_.initPendingState(handle1.state_);
    handle1.state_.initPendingState(handle0.state_);
    return {handle0: handle0, handle1: handle1};
  }

  function InterfaceEndpointHandle(interfaceId, associatedGroupController) {
    this.state_ = new State(interfaceId, associatedGroupController);
  }

  InterfaceEndpointHandle.prototype.isValid = function() {
    return this.state_.isValid();
  };

  InterfaceEndpointHandle.prototype.pendingAssociation = function() {
    return this.state_.pendingAssociation;
  };

  InterfaceEndpointHandle.prototype.id = function() {
    return this.state_.interfaceId;
  };

  InterfaceEndpointHandle.prototype.groupController = function() {
    return this.state_.associatedGroupController;
  };

  InterfaceEndpointHandle.prototype.disconnectReason = function() {
    return this.state_.disconnectReason;
  };

  InterfaceEndpointHandle.prototype.setAssociationEventHandler = function(
      handler) {
    this.state_.setAssociationEventHandler(handler);
  };

  InterfaceEndpointHandle.prototype.notifyAssociation = function(interfaceId,
      peerGroupController) {
    return this.state_.notifyAssociation(interfaceId, peerGroupController);
  };

  InterfaceEndpointHandle.prototype.reset = function(reason) {
    this.state_.close(reason);
    this.state_ = new State();
  };

  internal.AssociationEvent = AssociationEvent;
  internal.InterfaceEndpointHandle = InterfaceEndpointHandle;
  internal.createPairPendingAssociation = createPairPendingAssociation;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function validateControlRequestWithoutResponse(message) {
    var messageValidator = new internal.Validator(message);
    var error = messageValidator.validateMessageIsRequestWithoutResponse();
    if (error != internal.validationError.NONE) {
      throw error;
    }

    if (message.getName() != mojo.pipeControl2.kRunOrClosePipeMessageId) {
      throw new Error("Control message name is not kRunOrClosePipeMessageId");
    }

    // Validate payload.
    error = mojo.pipeControl2.RunOrClosePipeMessageParams.validate(
        messageValidator, message.getHeaderNumBytes());
    if (error != internal.validationError.NONE) {
      throw error;
    }
  }

  function runOrClosePipe(message, delegate) {
    var reader = new internal.MessageReader(message);
    var runOrClosePipeMessageParams = reader.decodeStruct(
        mojo.pipeControl2.RunOrClosePipeMessageParams);
    var event = runOrClosePipeMessageParams.input
        .peerAssociatedEndpointClosedEvent;
    return delegate.onPeerAssociatedEndpointClosed(event.id,
        event.disconnectReason);
  }

  function isPipeControlMessage(message) {
    return !internal.isValidInterfaceId(message.getInterfaceId());
  }

  function PipeControlMessageHandler(delegate) {
    this.delegate_ = delegate;
  }

  PipeControlMessageHandler.prototype.accept = function(message) {
    validateControlRequestWithoutResponse(message);
    return runOrClosePipe(message, this.delegate_);
  };

  internal.PipeControlMessageHandler = PipeControlMessageHandler;
  internal.isPipeControlMessage = isPipeControlMessage;
})();
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  function constructRunOrClosePipeMessage(runOrClosePipeInput) {
    var runOrClosePipeMessageParams = new
        mojo.pipeControl2.RunOrClosePipeMessageParams();
    runOrClosePipeMessageParams.input = runOrClosePipeInput;

    var messageName = mojo.pipeControl2.kRunOrClosePipeMessageId;
    var payloadSize =
        mojo.pipeControl2.RunOrClosePipeMessageParams.encodedSize;

    var builder = new internal.MessageV0Builder(messageName, payloadSize);
    builder.encodeStruct(mojo.pipeControl2.RunOrClosePipeMessageParams,
                         runOrClosePipeMessageParams);
    var message = builder.finish();
    message.setInterfaceId(internal.kInvalidInterfaceId);
    return message;
  }

  function PipeControlMessageProxy(receiver) {
    this.receiver_ = receiver;
  }

  PipeControlMessageProxy.prototype.notifyPeerEndpointClosed = function(
      interfaceId, reason) {
    var message = this.constructPeerEndpointClosedMessage(interfaceId, reason);
    this.receiver_.accept(message);
  };

  PipeControlMessageProxy.prototype.constructPeerEndpointClosedMessage =
      function(interfaceId, reason) {
    var event = new mojo.pipeControl2.PeerAssociatedEndpointClosedEvent();
    event.id = interfaceId;
    if (reason) {
      event.disconnectReason = new mojo.pipeControl2.DisconnectReason({
          customReason: reason.customReason,
          description: reason.description});
    }
    var runOrClosePipeInput = new mojo.pipeControl2.RunOrClosePipeInput();
    runOrClosePipeInput.peerAssociatedEndpointClosedEvent = event;
    return constructRunOrClosePipeMessage(runOrClosePipeInput);
  };

  internal.PipeControlMessageProxy = PipeControlMessageProxy;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  /**
   * The state of |endpoint|. If both the endpoint and its peer have been
   * closed, removes it from |endpoints_|.
   * @enum {string}
   */
  var EndpointStateUpdateType = {
    ENDPOINT_CLOSED: 'endpoint_closed',
    PEER_ENDPOINT_CLOSED: 'peer_endpoint_closed'
  };

  function check(condition, output) {
    if (!condition) {
      // testharness.js does not rethrow errors so the error stack needs to be
      // included as a string in the error we throw for debugging layout tests.
      throw new Error((new Error()).stack);
    }
  }

  function InterfaceEndpoint(router, interfaceId) {
    this.router_ = router;
    this.id = interfaceId;
    this.closed = false;
    this.peerClosed = false;
    this.handleCreated = false;
    this.disconnectReason = null;
    this.client = null;
  }

  InterfaceEndpoint.prototype.sendMessage = function(message) {
    message.setInterfaceId(this.id);
    return this.router_.connector_.accept(message);
  };

  function Router(handle, setInterfaceIdNamespaceBit) {
    if (!(handle instanceof MojoHandle)) {
      throw new Error("Router constructor: Not a handle");
    }
    if (setInterfaceIdNamespaceBit === undefined) {
      setInterfaceIdNamespaceBit = false;
    }

    this.connector_ = new internal.Connector(handle);

    this.connector_.setIncomingReceiver({
        accept: this.accept.bind(this),
    });
    this.connector_.setErrorHandler({
        onError: this.onPipeConnectionError.bind(this),
    });

    this.setInterfaceIdNamespaceBit_ = setInterfaceIdNamespaceBit;
    // |cachedMessageData| caches infomation about a message, so it can be
    // processed later if a client is not yet attached to the target endpoint.
    this.cachedMessageData = null;
    this.controlMessageHandler_ = new internal.PipeControlMessageHandler(this);
    this.controlMessageProxy_ =
        new internal.PipeControlMessageProxy(this.connector_);
    this.nextInterfaceIdValue_ = 1;
    this.encounteredError_ = false;
    this.endpoints_ = new Map();
  }

  Router.prototype.associateInterface = function(handleToSend) {
    if (!handleToSend.pendingAssociation()) {
      return internal.kInvalidInterfaceId;
    }

    var id = 0;
    do {
      if (this.nextInterfaceIdValue_ >= internal.kInterfaceIdNamespaceMask) {
        this.nextInterfaceIdValue_ = 1;
      }
      id = this.nextInterfaceIdValue_++;
      if (this.setInterfaceIdNamespaceBit_) {
        id += internal.kInterfaceIdNamespaceMask;
      }
    } while (this.endpoints_.has(id));

    var endpoint = new InterfaceEndpoint(this, id);
    this.endpoints_.set(id, endpoint);
    if (this.encounteredError_) {
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
    endpoint.handleCreated = true;

    if (!handleToSend.notifyAssociation(id, this)) {
      // The peer handle of |handleToSend|, which is supposed to join this
      // associated group, has been closed.
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.ENDPOINT_CLOSED);

      pipeControlMessageproxy.notifyPeerEndpointClosed(id,
          handleToSend.disconnectReason());
    }

    return id;
  };

  Router.prototype.attachEndpointClient = function(
      interfaceEndpointHandle, interfaceEndpointClient) {
    check(internal.isValidInterfaceId(interfaceEndpointHandle.id()));
    check(interfaceEndpointClient);

    var endpoint = this.endpoints_.get(interfaceEndpointHandle.id());
    check(endpoint);
    check(!endpoint.client);
    check(!endpoint.closed);
    endpoint.client = interfaceEndpointClient;

    if (endpoint.peerClosed) {
      setTimeout(endpoint.client.notifyError.bind(endpoint.client), 0);
    }

    if (this.cachedMessageData && interfaceEndpointHandle.id() ===
        this.cachedMessageData.message.getInterfaceId()) {
      setTimeout((function() {
        if (!this.cachedMessageData) {
          return;
        }

        var targetEndpoint = this.endpoints_.get(
            this.cachedMessageData.message.getInterfaceId());
        // Check that the target endpoint's client still exists.
        if (targetEndpoint && targetEndpoint.client) {
          var message = this.cachedMessageData.message;
          var messageValidator = this.cachedMessageData.messageValidator;
          this.cachedMessageData = null;
          this.connector_.resumeIncomingMethodCallProcessing();
          var ok = endpoint.client.handleIncomingMessage(message,
              messageValidator);

          // Handle invalid cached incoming message.
          if (!internal.isTestingMode() && !ok) {
            this.connector_.handleError(true, true);
          }
        }
      }).bind(this), 0);
    }

    return endpoint;
  };

  Router.prototype.detachEndpointClient = function(
      interfaceEndpointHandle) {
    check(internal.isValidInterfaceId(interfaceEndpointHandle.id()));
    var endpoint = this.endpoints_.get(interfaceEndpointHandle.id());
    check(endpoint);
    check(endpoint.client);
    check(!endpoint.closed);

    endpoint.client = null;
  };

  Router.prototype.createLocalEndpointHandle = function(
      interfaceId) {
    if (!internal.isValidInterfaceId(interfaceId)) {
      return new internal.InterfaceEndpointHandle();
    }

    var endpoint = this.endpoints_.get(interfaceId);

    if (!endpoint) {
      endpoint = new InterfaceEndpoint(this, interfaceId);
      this.endpoints_.set(interfaceId, endpoint);

      check(!endpoint.handleCreated);

      if (this.encounteredError_) {
        this.updateEndpointStateMayRemove(endpoint,
            EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
      }
    } else {
      // If the endpoint already exist, it is because we have received a
      // notification that the peer endpoint has closed.
      check(!endpoint.closed);
      check(endpoint.peerClosed);

      if (endpoint.handleCreated) {
        return new internal.InterfaceEndpointHandle();
      }
    }

    endpoint.handleCreated = true;
    return new internal.InterfaceEndpointHandle(interfaceId, this);
  };

  Router.prototype.accept = function(message) {
    var messageValidator = new internal.Validator(message);
    var err = messageValidator.validateMessageHeader();

    var ok = false;
    if (err !== internal.validationError.NONE) {
      internal.reportValidationError(err);
    } else if (message.deserializeAssociatedEndpointHandles(this)) {
      if (internal.isPipeControlMessage(message)) {
        ok = this.controlMessageHandler_.accept(message);
      } else {
        var interfaceId = message.getInterfaceId();
        var endpoint = this.endpoints_.get(interfaceId);
        if (!endpoint || endpoint.closed) {
          return true;
        }

        if (!endpoint.client) {
          // We need to wait until a client is attached in order to dispatch
          // further messages.
          this.cachedMessageData = {message: message,
              messageValidator: messageValidator};
          this.connector_.pauseIncomingMethodCallProcessing();
          return true;
        }
        ok = endpoint.client.handleIncomingMessage(message, messageValidator);
      }
    }
    return ok;
  };

  Router.prototype.close = function() {
    this.connector_.close();
    // Closing the message pipe won't trigger connection error handler.
    // Explicitly call onPipeConnectionError() so that associated endpoints
    // will get notified.
    this.onPipeConnectionError();
  };

  Router.prototype.onPeerAssociatedEndpointClosed = function(interfaceId,
      reason) {
    check(!internal.isMasterInterfaceId(interfaceId) || reason);

    var endpoint = this.endpoints_.get(interfaceId);
    if (!endpoint) {
      endpoint = new InterfaceEndpoint(this, interfaceId);
      this.endpoints_.set(interfaceId, endpoint);
    }

    if (reason) {
      endpoint.disconnectReason = reason;
    }

    if (!endpoint.peerClosed) {
      if (endpoint.client) {
        setTimeout(endpoint.client.notifyError.bind(endpoint.client, reason),
                   0);
      }
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
    return true;
  };

  Router.prototype.onPipeConnectionError = function() {
    this.encounteredError_ = true;

    for (var endpoint of this.endpoints_.values()) {
      if (endpoint.client) {
        setTimeout(
            endpoint.client.notifyError.bind(
                endpoint.client, endpoint.disconnectReason),
            0);
      }
      this.updateEndpointStateMayRemove(endpoint,
          EndpointStateUpdateType.PEER_ENDPOINT_CLOSED);
    }
  };

  Router.prototype.closeEndpointHandle = function(interfaceId, reason) {
    if (!internal.isValidInterfaceId(interfaceId)) {
      return;
    }
    var endpoint = this.endpoints_.get(interfaceId);
    check(endpoint);
    check(!endpoint.client);
    check(!endpoint.closed);

    this.updateEndpointStateMayRemove(endpoint,
        EndpointStateUpdateType.ENDPOINT_CLOSED);

    if (!internal.isMasterInterfaceId(interfaceId) || reason) {
      this.controlMessageProxy_.notifyPeerEndpointClosed(interfaceId, reason);
    }

    if (this.cachedMessageData && interfaceId ===
        this.cachedMessageData.message.getInterfaceId()) {
      this.cachedMessageData = null;
      this.connector_.resumeIncomingMethodCallProcessing();
    }
  };

  Router.prototype.updateEndpointStateMayRemove = function(endpoint,
      endpointStateUpdateType) {
    if (endpointStateUpdateType === EndpointStateUpdateType.ENDPOINT_CLOSED) {
      endpoint.closed = true;
    } else {
      endpoint.peerClosed = true;
    }
    if (endpoint.closed && endpoint.peerClosed) {
      this.endpoints_.delete(endpoint.id);
    }
  };

  internal.Router = Router;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Defines functions for translating between JavaScript strings and UTF8 strings
 * stored in ArrayBuffers. There is much room for optimization in this code if
 * it proves necessary.
 */
(function() {
  var internal = mojo.internal;

  /**
   * Decodes the UTF8 string from the given buffer.
   * @param {ArrayBufferView} buffer The buffer containing UTF8 string data.
   * @return {string} The corresponding JavaScript string.
   */
  function decodeUtf8String(buffer) {
    return decodeURIComponent(escape(String.fromCharCode.apply(null, buffer)));
  }

  /**
   * Encodes the given JavaScript string into UTF8.
   * @param {string} str The string to encode.
   * @param {ArrayBufferView} outputBuffer The buffer to contain the result.
   * Should be pre-allocated to hold enough space. Use |utf8Length| to determine
   * how much space is required.
   * @return {number} The number of bytes written to |outputBuffer|.
   */
  function encodeUtf8String(str, outputBuffer) {
    var utf8String = unescape(encodeURIComponent(str));
    if (outputBuffer.length < utf8String.length)
      throw new Error("Buffer too small for encodeUtf8String");
    for (var i = 0; i < outputBuffer.length && i < utf8String.length; i++)
      outputBuffer[i] = utf8String.charCodeAt(i);
    return i;
  }

  /**
   * Returns the number of bytes that a UTF8 encoding of the JavaScript string
   * |str| would occupy.
   */
  function utf8Length(str) {
    var utf8String = unescape(encodeURIComponent(str));
    return utf8String.length;
  }

  internal.decodeUtf8String = decodeUtf8String;
  internal.encodeUtf8String = encodeUtf8String;
  internal.utf8Length = utf8Length;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var internal = mojo.internal;

  var validationError = {
    NONE: 'VALIDATION_ERROR_NONE',
    MISALIGNED_OBJECT: 'VALIDATION_ERROR_MISALIGNED_OBJECT',
    ILLEGAL_MEMORY_RANGE: 'VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE',
    UNEXPECTED_STRUCT_HEADER: 'VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER',
    UNEXPECTED_ARRAY_HEADER: 'VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER',
    ILLEGAL_HANDLE: 'VALIDATION_ERROR_ILLEGAL_HANDLE',
    UNEXPECTED_INVALID_HANDLE: 'VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE',
    ILLEGAL_POINTER: 'VALIDATION_ERROR_ILLEGAL_POINTER',
    UNEXPECTED_NULL_POINTER: 'VALIDATION_ERROR_UNEXPECTED_NULL_POINTER',
    ILLEGAL_INTERFACE_ID: 'VALIDATION_ERROR_ILLEGAL_INTERFACE_ID',
    UNEXPECTED_INVALID_INTERFACE_ID:
        'VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID',
    MESSAGE_HEADER_INVALID_FLAGS:
        'VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS',
    MESSAGE_HEADER_MISSING_REQUEST_ID:
        'VALIDATION_ERROR_MESSAGE_HEADER_MISSING_REQUEST_ID',
    DIFFERENT_SIZED_ARRAYS_IN_MAP:
        'VALIDATION_ERROR_DIFFERENT_SIZED_ARRAYS_IN_MAP',
    INVALID_UNION_SIZE: 'VALIDATION_ERROR_INVALID_UNION_SIZE',
    UNEXPECTED_NULL_UNION: 'VALIDATION_ERROR_UNEXPECTED_NULL_UNION',
    UNKNOWN_ENUM_VALUE: 'VALIDATION_ERROR_UNKNOWN_ENUM_VALUE',
  };

  var NULL_MOJO_POINTER = "NULL_MOJO_POINTER";
  var gValidationErrorObserver = null;

  function reportValidationError(error) {
    if (gValidationErrorObserver) {
      gValidationErrorObserver.lastError = error;
    } else {
      console.warn('Invalid message: ' + error);
    }
  }

  var ValidationErrorObserverForTesting = (function() {
    function Observer() {
      this.lastError = validationError.NONE;
    }

    Observer.prototype.reset = function() {
      this.lastError = validationError.NONE;
    };

    return {
      getInstance: function() {
        if (!gValidationErrorObserver) {
          gValidationErrorObserver = new Observer();
        }
        return gValidationErrorObserver;
      }
    };
  })();

  function isTestingMode() {
    return Boolean(gValidationErrorObserver);
  }

  function clearTestingMode() {
    gValidationErrorObserver = null;
  }

  function isEnumClass(cls) {
    return cls instanceof internal.Enum;
  }

  function isStringClass(cls) {
    return cls === internal.String || cls === internal.NullableString;
  }

  function isHandleClass(cls) {
    return cls === internal.Handle || cls === internal.NullableHandle;
  }

  function isInterfaceClass(cls) {
    return cls instanceof internal.Interface;
  }

  function isInterfaceRequestClass(cls) {
    return cls === internal.InterfaceRequest ||
        cls === internal.NullableInterfaceRequest;
  }

  function isAssociatedInterfaceClass(cls) {
    return cls === internal.AssociatedInterfacePtrInfo ||
        cls === internal.NullableAssociatedInterfacePtrInfo;
  }

  function isAssociatedInterfaceRequestClass(cls) {
    return cls === internal.AssociatedInterfaceRequest ||
        cls === internal.NullableAssociatedInterfaceRequest;
  }

  function isNullable(type) {
    return type === internal.NullableString ||
        type === internal.NullableHandle ||
        type === internal.NullableAssociatedInterfacePtrInfo ||
        type === internal.NullableAssociatedInterfaceRequest ||
        type === internal.NullableInterface ||
        type === internal.NullableInterfaceRequest ||
        type instanceof internal.NullableArrayOf ||
        type instanceof internal.NullablePointerTo;
  }

  function Validator(message) {
    this.message = message;
    this.offset = 0;
    this.handleIndex = 0;
    this.associatedEndpointHandleIndex = 0;
    this.payloadInterfaceIds = null;
    this.offsetLimit = this.message.buffer.byteLength;
  }

  Object.defineProperty(Validator.prototype, "handleIndexLimit", {
    get: function() { return this.message.handles.length; }
  });

  Object.defineProperty(Validator.prototype, "associatedHandleIndexLimit", {
    get: function() {
      return this.payloadInterfaceIds ? this.payloadInterfaceIds.length : 0;
    }
  });

  // True if we can safely allocate a block of bytes from start to
  // to start + numBytes.
  Validator.prototype.isValidRange = function(start, numBytes) {
    // Only positive JavaScript integers that are less than 2^53
    // (Number.MAX_SAFE_INTEGER) can be represented exactly.
    if (start < this.offset || numBytes <= 0 ||
        !Number.isSafeInteger(start) ||
        !Number.isSafeInteger(numBytes))
      return false;

    var newOffset = start + numBytes;
    if (!Number.isSafeInteger(newOffset) || newOffset > this.offsetLimit)
      return false;

    return true;
  };

  Validator.prototype.claimRange = function(start, numBytes) {
    if (this.isValidRange(start, numBytes)) {
      this.offset = start + numBytes;
      return true;
    }
    return false;
  };

  Validator.prototype.claimHandle = function(index) {
    if (index === internal.kEncodedInvalidHandleValue)
      return true;

    if (index < this.handleIndex || index >= this.handleIndexLimit)
      return false;

    // This is safe because handle indices are uint32.
    this.handleIndex = index + 1;
    return true;
  };

  Validator.prototype.claimAssociatedEndpointHandle = function(index) {
    if (index === internal.kEncodedInvalidHandleValue) {
      return true;
    }

    if (index < this.associatedEndpointHandleIndex ||
        index >= this.associatedHandleIndexLimit) {
      return false;
    }

    // This is safe because handle indices are uint32.
    this.associatedEndpointHandleIndex = index + 1;
    return true;
  };

  Validator.prototype.validateEnum = function(offset, enumClass) {
    // Note: Assumes that enums are always 32 bits! But this matches
    // mojom::generate::pack::PackedField::GetSizeForKind, so it should be okay.
    var value = this.message.buffer.getInt32(offset);
    return enumClass.validate(value);
  }

  Validator.prototype.validateHandle = function(offset, nullable) {
    var index = this.message.buffer.getUint32(offset);

    if (index === internal.kEncodedInvalidHandleValue)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_INVALID_HANDLE;

    if (!this.claimHandle(index))
      return validationError.ILLEGAL_HANDLE;

    return validationError.NONE;
  };

  Validator.prototype.validateAssociatedEndpointHandle = function(offset,
      nullable) {
    var index = this.message.buffer.getUint32(offset);

    if (index === internal.kEncodedInvalidHandleValue) {
      return nullable ? validationError.NONE :
          validationError.UNEXPECTED_INVALID_INTERFACE_ID;
    }

    if (!this.claimAssociatedEndpointHandle(index)) {
      return validationError.ILLEGAL_INTERFACE_ID;
    }

    return validationError.NONE;
  };

  Validator.prototype.validateInterface = function(offset, nullable) {
    return this.validateHandle(offset, nullable);
  };

  Validator.prototype.validateInterfaceRequest = function(offset, nullable) {
    return this.validateHandle(offset, nullable);
  };

  Validator.prototype.validateAssociatedInterface = function(offset,
      nullable) {
    return this.validateAssociatedEndpointHandle(offset, nullable);
  };

  Validator.prototype.validateAssociatedInterfaceRequest = function(
      offset, nullable) {
    return this.validateAssociatedEndpointHandle(offset, nullable);
  };

  Validator.prototype.validateStructHeader = function(offset, minNumBytes) {
    if (!internal.isAligned(offset))
      return validationError.MISALIGNED_OBJECT;

    if (!this.isValidRange(offset, internal.kStructHeaderSize))
      return validationError.ILLEGAL_MEMORY_RANGE;

    var numBytes = this.message.buffer.getUint32(offset);

    if (numBytes < minNumBytes)
      return validationError.UNEXPECTED_STRUCT_HEADER;

    if (!this.claimRange(offset, numBytes))
      return validationError.ILLEGAL_MEMORY_RANGE;

    return validationError.NONE;
  };

  Validator.prototype.validateStructVersion = function(offset, versionSizes) {
    var numBytes = this.message.buffer.getUint32(offset);
    var version = this.message.buffer.getUint32(offset + 4);

    if (version <= versionSizes[versionSizes.length - 1].version) {
      // Scan in reverse order to optimize for more recent versionSizes.
      for (var i = versionSizes.length - 1; i >= 0; --i) {
        if (version >= versionSizes[i].version) {
          if (numBytes == versionSizes[i].numBytes)
            break;
          return validationError.UNEXPECTED_STRUCT_HEADER;
        }
      }
    } else if (numBytes < versionSizes[versionSizes.length-1].numBytes) {
      return validationError.UNEXPECTED_STRUCT_HEADER;
    }

    return validationError.NONE;
  };

  Validator.prototype.isFieldInStructVersion = function(offset, fieldVersion) {
    var structVersion = this.message.buffer.getUint32(offset + 4);
    return fieldVersion <= structVersion;
  };

  Validator.prototype.validateMessageHeader = function() {
    var err = this.validateStructHeader(0, internal.kMessageV0HeaderSize);
    if (err != validationError.NONE) {
      return err;
    }

    var numBytes = this.message.getHeaderNumBytes();
    var version = this.message.getHeaderVersion();

    var validVersionAndNumBytes =
        (version == 0 && numBytes == internal.kMessageV0HeaderSize) ||
        (version == 1 && numBytes == internal.kMessageV1HeaderSize) ||
        (version == 2 && numBytes == internal.kMessageV2HeaderSize) ||
        (version > 2 && numBytes >= internal.kMessageV2HeaderSize);

    if (!validVersionAndNumBytes) {
      return validationError.UNEXPECTED_STRUCT_HEADER;
    }

    var expectsResponse = this.message.expectsResponse();
    var isResponse = this.message.isResponse();

    if (version == 0 && (expectsResponse || isResponse)) {
      return validationError.MESSAGE_HEADER_MISSING_REQUEST_ID;
    }

    if (isResponse && expectsResponse) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }

    if (version < 2) {
      return validationError.NONE;
    }

    var err = this.validateArrayPointer(
        internal.kMessagePayloadInterfaceIdsPointerOffset,
        internal.Uint32.encodedSize, internal.Uint32, true, [0], 0);

    if (err != validationError.NONE) {
      return err;
    }

    this.payloadInterfaceIds = this.message.getPayloadInterfaceIds();
    if (this.payloadInterfaceIds) {
      for (var interfaceId of this.payloadInterfaceIds) {
        if (!internal.isValidInterfaceId(interfaceId) ||
            internal.isMasterInterfaceId(interfaceId)) {
          return validationError.ILLEGAL_INTERFACE_ID;
        }
      }
    }

    // Set offset to the start of the payload and offsetLimit to the start of
    // the payload interface Ids so that payload can be validated using the
    // same messageValidator.
    this.offset = this.message.getHeaderNumBytes();
    this.offsetLimit = this.decodePointer(
        internal.kMessagePayloadInterfaceIdsPointerOffset);

    return validationError.NONE;
  };

  Validator.prototype.validateMessageIsRequestWithoutResponse = function() {
    if (this.message.isResponse() || this.message.expectsResponse()) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateMessageIsRequestExpectingResponse = function() {
    if (this.message.isResponse() || !this.message.expectsResponse()) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateMessageIsResponse = function() {
    if (this.message.expectsResponse() || !this.message.isResponse()) {
      return validationError.MESSAGE_HEADER_INVALID_FLAGS;
    }
    return validationError.NONE;
  };

  // Returns the message.buffer relative offset this pointer "points to",
  // NULL_MOJO_POINTER if the pointer represents a null, or JS null if the
  // pointer's value is not valid.
  Validator.prototype.decodePointer = function(offset) {
    var pointerValue = this.message.buffer.getUint64(offset);
    if (pointerValue === 0)
      return NULL_MOJO_POINTER;
    var bufferOffset = offset + pointerValue;
    return Number.isSafeInteger(bufferOffset) ? bufferOffset : null;
  };

  Validator.prototype.decodeUnionSize = function(offset) {
    return this.message.buffer.getUint32(offset);
  };

  Validator.prototype.decodeUnionTag = function(offset) {
    return this.message.buffer.getUint32(offset + 4);
  };

  Validator.prototype.validateArrayPointer = function(
      offset, elementSize, elementType, nullable, expectedDimensionSizes,
      currentDimension) {
    var arrayOffset = this.decodePointer(offset);
    if (arrayOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (arrayOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    return this.validateArray(arrayOffset, elementSize, elementType,
                              expectedDimensionSizes, currentDimension);
  };

  Validator.prototype.validateStructPointer = function(
      offset, structClass, nullable) {
    var structOffset = this.decodePointer(offset);
    if (structOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (structOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    return structClass.validate(this, structOffset);
  };

  Validator.prototype.validateUnion = function(
      offset, unionClass, nullable) {
    var size = this.message.buffer.getUint32(offset);
    if (size == 0) {
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_UNION;
    }

    return unionClass.validate(this, offset);
  };

  Validator.prototype.validateNestedUnion = function(
      offset, unionClass, nullable) {
    var unionOffset = this.decodePointer(offset);
    if (unionOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (unionOffset === NULL_MOJO_POINTER)
      return nullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_UNION;

    return this.validateUnion(unionOffset, unionClass, nullable);
  };

  // This method assumes that the array at arrayPointerOffset has
  // been validated.

  Validator.prototype.arrayLength = function(arrayPointerOffset) {
    var arrayOffset = this.decodePointer(arrayPointerOffset);
    return this.message.buffer.getUint32(arrayOffset + 4);
  };

  Validator.prototype.validateMapPointer = function(
      offset, mapIsNullable, keyClass, valueClass, valueIsNullable) {
    // Validate the implicit map struct:
    // struct {array<keyClass> keys; array<valueClass> values};
    var structOffset = this.decodePointer(offset);
    if (structOffset === null)
      return validationError.ILLEGAL_POINTER;

    if (structOffset === NULL_MOJO_POINTER)
      return mapIsNullable ?
          validationError.NONE : validationError.UNEXPECTED_NULL_POINTER;

    var mapEncodedSize = internal.kStructHeaderSize +
                         internal.kMapStructPayloadSize;
    var err = this.validateStructHeader(structOffset, mapEncodedSize);
    if (err !== validationError.NONE)
        return err;

    // Validate the keys array.
    var keysArrayPointerOffset = structOffset + internal.kStructHeaderSize;
    err = this.validateArrayPointer(
        keysArrayPointerOffset, keyClass.encodedSize, keyClass, false, [0], 0);
    if (err !== validationError.NONE)
        return err;

    // Validate the values array.
    var valuesArrayPointerOffset = keysArrayPointerOffset + 8;
    var valuesArrayDimensions = [0]; // Validate the actual length below.
    if (valueClass instanceof internal.ArrayOf)
      valuesArrayDimensions =
          valuesArrayDimensions.concat(valueClass.dimensions());
    var err = this.validateArrayPointer(valuesArrayPointerOffset,
                                        valueClass.encodedSize,
                                        valueClass,
                                        valueIsNullable,
                                        valuesArrayDimensions,
                                        0);
    if (err !== validationError.NONE)
        return err;

    // Validate the lengths of the keys and values arrays.
    var keysArrayLength = this.arrayLength(keysArrayPointerOffset);
    var valuesArrayLength = this.arrayLength(valuesArrayPointerOffset);
    if (keysArrayLength != valuesArrayLength)
      return validationError.DIFFERENT_SIZED_ARRAYS_IN_MAP;

    return validationError.NONE;
  };

  Validator.prototype.validateStringPointer = function(offset, nullable) {
    return this.validateArrayPointer(
        offset, internal.Uint8.encodedSize, internal.Uint8, nullable, [0], 0);
  };

  // Similar to Array_Data<T>::Validate()
  // mojo/public/cpp/bindings/lib/array_internal.h

  Validator.prototype.validateArray =
      function (offset, elementSize, elementType, expectedDimensionSizes,
                currentDimension) {
    if (!internal.isAligned(offset))
      return validationError.MISALIGNED_OBJECT;

    if (!this.isValidRange(offset, internal.kArrayHeaderSize))
      return validationError.ILLEGAL_MEMORY_RANGE;

    var numBytes = this.message.buffer.getUint32(offset);
    var numElements = this.message.buffer.getUint32(offset + 4);

    // Note: this computation is "safe" because elementSize <= 8 and
    // numElements is a uint32.
    var elementsTotalSize = (elementType === internal.PackedBool) ?
        Math.ceil(numElements / 8) : (elementSize * numElements);

    if (numBytes < internal.kArrayHeaderSize + elementsTotalSize)
      return validationError.UNEXPECTED_ARRAY_HEADER;

    if (expectedDimensionSizes[currentDimension] != 0 &&
        numElements != expectedDimensionSizes[currentDimension]) {
      return validationError.UNEXPECTED_ARRAY_HEADER;
    }

    if (!this.claimRange(offset, numBytes))
      return validationError.ILLEGAL_MEMORY_RANGE;

    // Validate the array's elements if they are pointers or handles.

    var elementsOffset = offset + internal.kArrayHeaderSize;
    var nullable = isNullable(elementType);

    if (isHandleClass(elementType))
      return this.validateHandleElements(elementsOffset, numElements, nullable);
    if (isInterfaceClass(elementType))
      return this.validateInterfaceElements(
          elementsOffset, numElements, nullable);
    if (isInterfaceRequestClass(elementType))
      return this.validateInterfaceRequestElements(
          elementsOffset, numElements, nullable);
    if (isAssociatedInterfaceClass(elementType))
      return this.validateAssociatedInterfaceElements(
          elementsOffset, numElements, nullable);
    if (isAssociatedInterfaceRequestClass(elementType))
      return this.validateAssociatedInterfaceRequestElements(
          elementsOffset, numElements, nullable);
    if (isStringClass(elementType))
      return this.validateArrayElements(
          elementsOffset, numElements, internal.Uint8, nullable, [0], 0);
    if (elementType instanceof internal.PointerTo)
      return this.validateStructElements(
          elementsOffset, numElements, elementType.cls, nullable);
    if (elementType instanceof internal.ArrayOf)
      return this.validateArrayElements(
          elementsOffset, numElements, elementType.cls, nullable,
          expectedDimensionSizes, currentDimension + 1);
    if (isEnumClass(elementType))
      return this.validateEnumElements(elementsOffset, numElements,
                                       elementType.cls);

    return validationError.NONE;
  };

  // Note: the |offset + i * elementSize| computation in the validateFooElements
  // methods below is "safe" because elementSize <= 8, offset and
  // numElements are uint32, and 0 <= i < numElements.

  Validator.prototype.validateHandleElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.Handle.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateHandle(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateInterfaceElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.Interface.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateInterface(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateInterfaceRequestElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.InterfaceRequest.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateInterfaceRequest(elementOffset, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateAssociatedInterfaceElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.AssociatedInterfacePtrInfo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateAssociatedInterface(elementOffset, nullable);
      if (err != validationError.NONE) {
        return err;
      }
    }
    return validationError.NONE;
  };

  Validator.prototype.validateAssociatedInterfaceRequestElements =
      function(offset, numElements, nullable) {
    var elementSize = internal.AssociatedInterfaceRequest.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateAssociatedInterfaceRequest(elementOffset,
          nullable);
      if (err != validationError.NONE) {
        return err;
      }
    }
    return validationError.NONE;
  };

  // The elementClass parameter is the element type of the element arrays.
  Validator.prototype.validateArrayElements =
      function(offset, numElements, elementClass, nullable,
               expectedDimensionSizes, currentDimension) {
    var elementSize = internal.PointerTo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateArrayPointer(
          elementOffset, elementClass.encodedSize, elementClass, nullable,
          expectedDimensionSizes, currentDimension);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateStructElements =
      function(offset, numElements, structClass, nullable) {
    var elementSize = internal.PointerTo.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err =
          this.validateStructPointer(elementOffset, structClass, nullable);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  Validator.prototype.validateEnumElements =
      function(offset, numElements, enumClass) {
    var elementSize = internal.Enum.prototype.encodedSize;
    for (var i = 0; i < numElements; i++) {
      var elementOffset = offset + i * elementSize;
      var err = this.validateEnum(elementOffset, enumClass);
      if (err != validationError.NONE)
        return err;
    }
    return validationError.NONE;
  };

  internal.validationError = validationError;
  internal.Validator = Validator;
  internal.ValidationErrorObserverForTesting =
      ValidationErrorObserverForTesting;
  internal.reportValidationError = reportValidationError;
  internal.isTestingMode = isTestingMode;
  internal.clearTestingMode = clearTestingMode;
})();
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';

(function() {
  var mojomId = 'mojo/public/interfaces/bindings/new_bindings/interface_control_messages.mojom';
  if (mojo.internal.isMojomLoaded(mojomId)) {
    console.warn('The following mojom is loaded multiple times: ' + mojomId);
    return;
  }
  mojo.internal.markMojomLoaded(mojomId);

  // TODO(yzshen): Define these aliases to minimize the differences between the
  // old/new modes. Remove them when the old mode goes away.
  var bindings = mojo;
  var associatedBindings = mojo;
  var codec = mojo.internal;
  var validator = mojo.internal;

  var exports = mojo.internal.exposeNamespace('mojo.interfaceControl2');


  var kRunMessageId = 0xFFFFFFFF;
  var kRunOrClosePipeMessageId = 0xFFFFFFFE;

  function RunMessageParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  RunMessageParams.prototype.initDefaults_ = function() {
    this.input = null;
  };
  RunMessageParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  RunMessageParams.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate RunMessageParams.input
    err = messageValidator.validateUnion(offset + codec.kStructHeaderSize + 0, RunInput, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  RunMessageParams.encodedSize = codec.kStructHeaderSize + 16;

  RunMessageParams.decode = function(decoder) {
    var packed;
    var val = new RunMessageParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.input = decoder.decodeStruct(RunInput);
    return val;
  };

  RunMessageParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(RunMessageParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(RunInput, val.input);
  };
  function RunResponseMessageParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  RunResponseMessageParams.prototype.initDefaults_ = function() {
    this.output = null;
  };
  RunResponseMessageParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  RunResponseMessageParams.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate RunResponseMessageParams.output
    err = messageValidator.validateUnion(offset + codec.kStructHeaderSize + 0, RunOutput, true);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  RunResponseMessageParams.encodedSize = codec.kStructHeaderSize + 16;

  RunResponseMessageParams.decode = function(decoder) {
    var packed;
    var val = new RunResponseMessageParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.output = decoder.decodeStruct(RunOutput);
    return val;
  };

  RunResponseMessageParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(RunResponseMessageParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(RunOutput, val.output);
  };
  function QueryVersion(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  QueryVersion.prototype.initDefaults_ = function() {
  };
  QueryVersion.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  QueryVersion.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 8}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  QueryVersion.encodedSize = codec.kStructHeaderSize + 0;

  QueryVersion.decode = function(decoder) {
    var packed;
    var val = new QueryVersion();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  QueryVersion.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(QueryVersion.encodedSize);
    encoder.writeUint32(0);
  };
  function QueryVersionResult(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  QueryVersionResult.prototype.initDefaults_ = function() {
    this.version = 0;
  };
  QueryVersionResult.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  QueryVersionResult.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 16}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    return validator.validationError.NONE;
  };

  QueryVersionResult.encodedSize = codec.kStructHeaderSize + 8;

  QueryVersionResult.decode = function(decoder) {
    var packed;
    var val = new QueryVersionResult();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.version = decoder.decodeStruct(codec.Uint32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  QueryVersionResult.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(QueryVersionResult.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Uint32, val.version);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };
  function FlushForTesting(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  FlushForTesting.prototype.initDefaults_ = function() {
  };
  FlushForTesting.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  FlushForTesting.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 8}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  FlushForTesting.encodedSize = codec.kStructHeaderSize + 0;

  FlushForTesting.decode = function(decoder) {
    var packed;
    var val = new FlushForTesting();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    return val;
  };

  FlushForTesting.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(FlushForTesting.encodedSize);
    encoder.writeUint32(0);
  };
  function RunOrClosePipeMessageParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  RunOrClosePipeMessageParams.prototype.initDefaults_ = function() {
    this.input = null;
  };
  RunOrClosePipeMessageParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  RunOrClosePipeMessageParams.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate RunOrClosePipeMessageParams.input
    err = messageValidator.validateUnion(offset + codec.kStructHeaderSize + 0, RunOrClosePipeInput, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  RunOrClosePipeMessageParams.encodedSize = codec.kStructHeaderSize + 16;

  RunOrClosePipeMessageParams.decode = function(decoder) {
    var packed;
    var val = new RunOrClosePipeMessageParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.input = decoder.decodeStruct(RunOrClosePipeInput);
    return val;
  };

  RunOrClosePipeMessageParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(RunOrClosePipeMessageParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(RunOrClosePipeInput, val.input);
  };
  function RequireVersion(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  RequireVersion.prototype.initDefaults_ = function() {
    this.version = 0;
  };
  RequireVersion.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  RequireVersion.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 16}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    return validator.validationError.NONE;
  };

  RequireVersion.encodedSize = codec.kStructHeaderSize + 8;

  RequireVersion.decode = function(decoder) {
    var packed;
    var val = new RequireVersion();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.version = decoder.decodeStruct(codec.Uint32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    return val;
  };

  RequireVersion.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(RequireVersion.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Uint32, val.version);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
  };

  function RunInput(value) {
    this.initDefault_();
    this.initValue_(value);
  }
  
  
  RunInput.Tags = {
    queryVersion: 0,
    flushForTesting: 1,
  };
  
  RunInput.prototype.initDefault_ = function() {
    this.$data = null;
    this.$tag = undefined;
  }
  
  RunInput.prototype.initValue_ = function(value) {
    if (value == undefined) {
      return;
    }
  
    var keys = Object.keys(value);
    if (keys.length == 0) {
      return;
    }
  
    if (keys.length > 1) {
      throw new TypeError("You may set only one member on a union.");
    }
  
    var fields = [
        "queryVersion",
        "flushForTesting",
    ];
  
    if (fields.indexOf(keys[0]) < 0) {
      throw new ReferenceError(keys[0] + " is not a RunInput member.");
  
    }
  
    this[keys[0]] = value[keys[0]];
  }
  Object.defineProperty(RunInput.prototype, "queryVersion", {
    get: function() {
      if (this.$tag != RunInput.Tags.queryVersion) {
        throw new ReferenceError(
            "RunInput.queryVersion is not currently set.");
      }
      return this.$data;
    },
  
    set: function(value) {
      this.$tag = RunInput.Tags.queryVersion;
      this.$data = value;
    }
  });
  Object.defineProperty(RunInput.prototype, "flushForTesting", {
    get: function() {
      if (this.$tag != RunInput.Tags.flushForTesting) {
        throw new ReferenceError(
            "RunInput.flushForTesting is not currently set.");
      }
      return this.$data;
    },
  
    set: function(value) {
      this.$tag = RunInput.Tags.flushForTesting;
      this.$data = value;
    }
  });
  
  
    RunInput.encode = function(encoder, val) {
      if (val == null) {
        encoder.writeUint64(0);
        encoder.writeUint64(0);
        return;
      }
      if (val.$tag == undefined) {
        throw new TypeError("Cannot encode unions with an unknown member set.");
      }
    
      encoder.writeUint32(16);
      encoder.writeUint32(val.$tag);
      switch (val.$tag) {
        case RunInput.Tags.queryVersion:
          encoder.encodeStructPointer(QueryVersion, val.queryVersion);
          break;
        case RunInput.Tags.flushForTesting:
          encoder.encodeStructPointer(FlushForTesting, val.flushForTesting);
          break;
      }
      encoder.align();
    };
  
  
    RunInput.decode = function(decoder) {
      var size = decoder.readUint32();
      if (size == 0) {
        decoder.readUint32();
        decoder.readUint64();
        return null;
      }
    
      var result = new RunInput();
      var tag = decoder.readUint32();
      switch (tag) {
        case RunInput.Tags.queryVersion:
          result.queryVersion = decoder.decodeStructPointer(QueryVersion);
          break;
        case RunInput.Tags.flushForTesting:
          result.flushForTesting = decoder.decodeStructPointer(FlushForTesting);
          break;
      }
      decoder.align();
    
      return result;
    };
  
  
    RunInput.validate = function(messageValidator, offset) {
      var size = messageValidator.decodeUnionSize(offset);
      if (size != 16) {
        return validator.validationError.INVALID_UNION_SIZE;
      }
    
      var tag = messageValidator.decodeUnionTag(offset);
      var data_offset = offset + 8;
      var err;
      switch (tag) {
        case RunInput.Tags.queryVersion:
          
    
    // validate RunInput.queryVersion
    err = messageValidator.validateStructPointer(data_offset, QueryVersion, false);
    if (err !== validator.validationError.NONE)
        return err;
          break;
        case RunInput.Tags.flushForTesting:
          
    
    // validate RunInput.flushForTesting
    err = messageValidator.validateStructPointer(data_offset, FlushForTesting, false);
    if (err !== validator.validationError.NONE)
        return err;
          break;
      }
    
      return validator.validationError.NONE;
    };
  
  RunInput.encodedSize = 16;

  function RunOutput(value) {
    this.initDefault_();
    this.initValue_(value);
  }
  
  
  RunOutput.Tags = {
    queryVersionResult: 0,
  };
  
  RunOutput.prototype.initDefault_ = function() {
    this.$data = null;
    this.$tag = undefined;
  }
  
  RunOutput.prototype.initValue_ = function(value) {
    if (value == undefined) {
      return;
    }
  
    var keys = Object.keys(value);
    if (keys.length == 0) {
      return;
    }
  
    if (keys.length > 1) {
      throw new TypeError("You may set only one member on a union.");
    }
  
    var fields = [
        "queryVersionResult",
    ];
  
    if (fields.indexOf(keys[0]) < 0) {
      throw new ReferenceError(keys[0] + " is not a RunOutput member.");
  
    }
  
    this[keys[0]] = value[keys[0]];
  }
  Object.defineProperty(RunOutput.prototype, "queryVersionResult", {
    get: function() {
      if (this.$tag != RunOutput.Tags.queryVersionResult) {
        throw new ReferenceError(
            "RunOutput.queryVersionResult is not currently set.");
      }
      return this.$data;
    },
  
    set: function(value) {
      this.$tag = RunOutput.Tags.queryVersionResult;
      this.$data = value;
    }
  });
  
  
    RunOutput.encode = function(encoder, val) {
      if (val == null) {
        encoder.writeUint64(0);
        encoder.writeUint64(0);
        return;
      }
      if (val.$tag == undefined) {
        throw new TypeError("Cannot encode unions with an unknown member set.");
      }
    
      encoder.writeUint32(16);
      encoder.writeUint32(val.$tag);
      switch (val.$tag) {
        case RunOutput.Tags.queryVersionResult:
          encoder.encodeStructPointer(QueryVersionResult, val.queryVersionResult);
          break;
      }
      encoder.align();
    };
  
  
    RunOutput.decode = function(decoder) {
      var size = decoder.readUint32();
      if (size == 0) {
        decoder.readUint32();
        decoder.readUint64();
        return null;
      }
    
      var result = new RunOutput();
      var tag = decoder.readUint32();
      switch (tag) {
        case RunOutput.Tags.queryVersionResult:
          result.queryVersionResult = decoder.decodeStructPointer(QueryVersionResult);
          break;
      }
      decoder.align();
    
      return result;
    };
  
  
    RunOutput.validate = function(messageValidator, offset) {
      var size = messageValidator.decodeUnionSize(offset);
      if (size != 16) {
        return validator.validationError.INVALID_UNION_SIZE;
      }
    
      var tag = messageValidator.decodeUnionTag(offset);
      var data_offset = offset + 8;
      var err;
      switch (tag) {
        case RunOutput.Tags.queryVersionResult:
          
    
    // validate RunOutput.queryVersionResult
    err = messageValidator.validateStructPointer(data_offset, QueryVersionResult, false);
    if (err !== validator.validationError.NONE)
        return err;
          break;
      }
    
      return validator.validationError.NONE;
    };
  
  RunOutput.encodedSize = 16;

  function RunOrClosePipeInput(value) {
    this.initDefault_();
    this.initValue_(value);
  }
  
  
  RunOrClosePipeInput.Tags = {
    requireVersion: 0,
  };
  
  RunOrClosePipeInput.prototype.initDefault_ = function() {
    this.$data = null;
    this.$tag = undefined;
  }
  
  RunOrClosePipeInput.prototype.initValue_ = function(value) {
    if (value == undefined) {
      return;
    }
  
    var keys = Object.keys(value);
    if (keys.length == 0) {
      return;
    }
  
    if (keys.length > 1) {
      throw new TypeError("You may set only one member on a union.");
    }
  
    var fields = [
        "requireVersion",
    ];
  
    if (fields.indexOf(keys[0]) < 0) {
      throw new ReferenceError(keys[0] + " is not a RunOrClosePipeInput member.");
  
    }
  
    this[keys[0]] = value[keys[0]];
  }
  Object.defineProperty(RunOrClosePipeInput.prototype, "requireVersion", {
    get: function() {
      if (this.$tag != RunOrClosePipeInput.Tags.requireVersion) {
        throw new ReferenceError(
            "RunOrClosePipeInput.requireVersion is not currently set.");
      }
      return this.$data;
    },
  
    set: function(value) {
      this.$tag = RunOrClosePipeInput.Tags.requireVersion;
      this.$data = value;
    }
  });
  
  
    RunOrClosePipeInput.encode = function(encoder, val) {
      if (val == null) {
        encoder.writeUint64(0);
        encoder.writeUint64(0);
        return;
      }
      if (val.$tag == undefined) {
        throw new TypeError("Cannot encode unions with an unknown member set.");
      }
    
      encoder.writeUint32(16);
      encoder.writeUint32(val.$tag);
      switch (val.$tag) {
        case RunOrClosePipeInput.Tags.requireVersion:
          encoder.encodeStructPointer(RequireVersion, val.requireVersion);
          break;
      }
      encoder.align();
    };
  
  
    RunOrClosePipeInput.decode = function(decoder) {
      var size = decoder.readUint32();
      if (size == 0) {
        decoder.readUint32();
        decoder.readUint64();
        return null;
      }
    
      var result = new RunOrClosePipeInput();
      var tag = decoder.readUint32();
      switch (tag) {
        case RunOrClosePipeInput.Tags.requireVersion:
          result.requireVersion = decoder.decodeStructPointer(RequireVersion);
          break;
      }
      decoder.align();
    
      return result;
    };
  
  
    RunOrClosePipeInput.validate = function(messageValidator, offset) {
      var size = messageValidator.decodeUnionSize(offset);
      if (size != 16) {
        return validator.validationError.INVALID_UNION_SIZE;
      }
    
      var tag = messageValidator.decodeUnionTag(offset);
      var data_offset = offset + 8;
      var err;
      switch (tag) {
        case RunOrClosePipeInput.Tags.requireVersion:
          
    
    // validate RunOrClosePipeInput.requireVersion
    err = messageValidator.validateStructPointer(data_offset, RequireVersion, false);
    if (err !== validator.validationError.NONE)
        return err;
          break;
      }
    
      return validator.validationError.NONE;
    };
  
  RunOrClosePipeInput.encodedSize = 16;
  exports.kRunMessageId = kRunMessageId;
  exports.kRunOrClosePipeMessageId = kRunOrClosePipeMessageId;
  exports.RunMessageParams = RunMessageParams;
  exports.RunResponseMessageParams = RunResponseMessageParams;
  exports.QueryVersion = QueryVersion;
  exports.QueryVersionResult = QueryVersionResult;
  exports.FlushForTesting = FlushForTesting;
  exports.RunOrClosePipeMessageParams = RunOrClosePipeMessageParams;
  exports.RequireVersion = RequireVersion;
  exports.RunInput = RunInput;
  exports.RunOutput = RunOutput;
  exports.RunOrClosePipeInput = RunOrClosePipeInput;
})();// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';

(function() {
  var mojomId = 'mojo/public/interfaces/bindings/new_bindings/pipe_control_messages.mojom';
  if (mojo.internal.isMojomLoaded(mojomId)) {
    console.warn('The following mojom is loaded multiple times: ' + mojomId);
    return;
  }
  mojo.internal.markMojomLoaded(mojomId);

  // TODO(yzshen): Define these aliases to minimize the differences between the
  // old/new modes. Remove them when the old mode goes away.
  var bindings = mojo;
  var associatedBindings = mojo;
  var codec = mojo.internal;
  var validator = mojo.internal;

  var exports = mojo.internal.exposeNamespace('mojo.pipeControl2');


  var kRunOrClosePipeMessageId = 0xFFFFFFFE;

  function RunOrClosePipeMessageParams(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  RunOrClosePipeMessageParams.prototype.initDefaults_ = function() {
    this.input = null;
  };
  RunOrClosePipeMessageParams.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  RunOrClosePipeMessageParams.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;


    // validate RunOrClosePipeMessageParams.input
    err = messageValidator.validateUnion(offset + codec.kStructHeaderSize + 0, RunOrClosePipeInput, false);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  RunOrClosePipeMessageParams.encodedSize = codec.kStructHeaderSize + 16;

  RunOrClosePipeMessageParams.decode = function(decoder) {
    var packed;
    var val = new RunOrClosePipeMessageParams();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.input = decoder.decodeStruct(RunOrClosePipeInput);
    return val;
  };

  RunOrClosePipeMessageParams.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(RunOrClosePipeMessageParams.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(RunOrClosePipeInput, val.input);
  };
  function DisconnectReason(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  DisconnectReason.prototype.initDefaults_ = function() {
    this.customReason = 0;
    this.description = null;
  };
  DisconnectReason.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  DisconnectReason.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;



    
    // validate DisconnectReason.description
    err = messageValidator.validateStringPointer(offset + codec.kStructHeaderSize + 8, false)
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  DisconnectReason.encodedSize = codec.kStructHeaderSize + 16;

  DisconnectReason.decode = function(decoder) {
    var packed;
    var val = new DisconnectReason();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.customReason = decoder.decodeStruct(codec.Uint32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    val.description = decoder.decodeStruct(codec.String);
    return val;
  };

  DisconnectReason.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(DisconnectReason.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Uint32, val.customReason);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.encodeStruct(codec.String, val.description);
  };
  function PeerAssociatedEndpointClosedEvent(values) {
    this.initDefaults_();
    this.initFields_(values);
  }


  PeerAssociatedEndpointClosedEvent.prototype.initDefaults_ = function() {
    this.id = 0;
    this.disconnectReason = null;
  };
  PeerAssociatedEndpointClosedEvent.prototype.initFields_ = function(fields) {
    for(var field in fields) {
        if (this.hasOwnProperty(field))
          this[field] = fields[field];
    }
  };

  PeerAssociatedEndpointClosedEvent.validate = function(messageValidator, offset) {
    var err;
    err = messageValidator.validateStructHeader(offset, codec.kStructHeaderSize);
    if (err !== validator.validationError.NONE)
        return err;

    var kVersionSizes = [
      {version: 0, numBytes: 24}
    ];
    err = messageValidator.validateStructVersion(offset, kVersionSizes);
    if (err !== validator.validationError.NONE)
        return err;



    
    // validate PeerAssociatedEndpointClosedEvent.disconnectReason
    err = messageValidator.validateStructPointer(offset + codec.kStructHeaderSize + 8, DisconnectReason, true);
    if (err !== validator.validationError.NONE)
        return err;

    return validator.validationError.NONE;
  };

  PeerAssociatedEndpointClosedEvent.encodedSize = codec.kStructHeaderSize + 16;

  PeerAssociatedEndpointClosedEvent.decode = function(decoder) {
    var packed;
    var val = new PeerAssociatedEndpointClosedEvent();
    var numberOfBytes = decoder.readUint32();
    var version = decoder.readUint32();
    val.id = decoder.decodeStruct(codec.Uint32);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    decoder.skip(1);
    val.disconnectReason = decoder.decodeStructPointer(DisconnectReason);
    return val;
  };

  PeerAssociatedEndpointClosedEvent.encode = function(encoder, val) {
    var packed;
    encoder.writeUint32(PeerAssociatedEndpointClosedEvent.encodedSize);
    encoder.writeUint32(0);
    encoder.encodeStruct(codec.Uint32, val.id);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.skip(1);
    encoder.encodeStructPointer(DisconnectReason, val.disconnectReason);
  };

  function RunOrClosePipeInput(value) {
    this.initDefault_();
    this.initValue_(value);
  }
  
  
  RunOrClosePipeInput.Tags = {
    peerAssociatedEndpointClosedEvent: 0,
  };
  
  RunOrClosePipeInput.prototype.initDefault_ = function() {
    this.$data = null;
    this.$tag = undefined;
  }
  
  RunOrClosePipeInput.prototype.initValue_ = function(value) {
    if (value == undefined) {
      return;
    }
  
    var keys = Object.keys(value);
    if (keys.length == 0) {
      return;
    }
  
    if (keys.length > 1) {
      throw new TypeError("You may set only one member on a union.");
    }
  
    var fields = [
        "peerAssociatedEndpointClosedEvent",
    ];
  
    if (fields.indexOf(keys[0]) < 0) {
      throw new ReferenceError(keys[0] + " is not a RunOrClosePipeInput member.");
  
    }
  
    this[keys[0]] = value[keys[0]];
  }
  Object.defineProperty(RunOrClosePipeInput.prototype, "peerAssociatedEndpointClosedEvent", {
    get: function() {
      if (this.$tag != RunOrClosePipeInput.Tags.peerAssociatedEndpointClosedEvent) {
        throw new ReferenceError(
            "RunOrClosePipeInput.peerAssociatedEndpointClosedEvent is not currently set.");
      }
      return this.$data;
    },
  
    set: function(value) {
      this.$tag = RunOrClosePipeInput.Tags.peerAssociatedEndpointClosedEvent;
      this.$data = value;
    }
  });
  
  
    RunOrClosePipeInput.encode = function(encoder, val) {
      if (val == null) {
        encoder.writeUint64(0);
        encoder.writeUint64(0);
        return;
      }
      if (val.$tag == undefined) {
        throw new TypeError("Cannot encode unions with an unknown member set.");
      }
    
      encoder.writeUint32(16);
      encoder.writeUint32(val.$tag);
      switch (val.$tag) {
        case RunOrClosePipeInput.Tags.peerAssociatedEndpointClosedEvent:
          encoder.encodeStructPointer(PeerAssociatedEndpointClosedEvent, val.peerAssociatedEndpointClosedEvent);
          break;
      }
      encoder.align();
    };
  
  
    RunOrClosePipeInput.decode = function(decoder) {
      var size = decoder.readUint32();
      if (size == 0) {
        decoder.readUint32();
        decoder.readUint64();
        return null;
      }
    
      var result = new RunOrClosePipeInput();
      var tag = decoder.readUint32();
      switch (tag) {
        case RunOrClosePipeInput.Tags.peerAssociatedEndpointClosedEvent:
          result.peerAssociatedEndpointClosedEvent = decoder.decodeStructPointer(PeerAssociatedEndpointClosedEvent);
          break;
      }
      decoder.align();
    
      return result;
    };
  
  
    RunOrClosePipeInput.validate = function(messageValidator, offset) {
      var size = messageValidator.decodeUnionSize(offset);
      if (size != 16) {
        return validator.validationError.INVALID_UNION_SIZE;
      }
    
      var tag = messageValidator.decodeUnionTag(offset);
      var data_offset = offset + 8;
      var err;
      switch (tag) {
        case RunOrClosePipeInput.Tags.peerAssociatedEndpointClosedEvent:
          
    
    // validate RunOrClosePipeInput.peerAssociatedEndpointClosedEvent
    err = messageValidator.validateStructPointer(data_offset, PeerAssociatedEndpointClosedEvent, false);
    if (err !== validator.validationError.NONE)
        return err;
          break;
      }
    
      return validator.validationError.NONE;
    };
  
  RunOrClosePipeInput.encodedSize = 16;
  exports.kRunOrClosePipeMessageId = kRunOrClosePipeMessageId;
  exports.RunOrClosePipeMessageParams = RunOrClosePipeMessageParams;
  exports.DisconnectReason = DisconnectReason;
  exports.PeerAssociatedEndpointClosedEvent = PeerAssociatedEndpointClosedEvent;
  exports.RunOrClosePipeInput = RunOrClosePipeInput;
})();