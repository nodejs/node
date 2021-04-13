declare namespace InternalQuicBinding {

  // TODO(@jasnell): This should be in the net namespace
  interface SocketAddress {}

  // TODO(@jasnell): These should be in the crypto namespace
  interface SecureContext {}
  interface X509Certificate {}

  interface ConfigObjectInit {
    retryTokenExpiration: bigint | number;
    tokenExiration: bigint | number;
    maxWindowOverride: bigint | number;
    maxStreamWindowOverride: bigint | number;
    maxConnectionsPerHost: bigint | number;
    maxConnectionsTotal: bigint | number;
    maxStatelessResets: bigint | number,
    addressLRUSize: bigint | number,
    retryLimit: bigint | number,
    maxPayloadSize: bigint | number,
    unacknowledgedPacketThreshold: bigint | number,
    validateAddress: boolean,
    disableStatelessReset: boolean,
    rxPacketLoss: number,
    txPacketLoss: number,
    ccAlgorithm: string,
    ipv6Only: boolean,
    receiveBufferSize: number,
    sendBufferSize: number,
    ttl: number,
  }

  class JSQuicBufferConsumer {}
  class ArrayBufferViewSource {}
  class StreamSource {}
  class StreamBaseSource {}

  class ConfigObject {
    constructor(address: SocketAddress, config: ConfigObjectInit);
    generateResetTokenSecret() : void;
    setResetTokenSecret(secret : ArrayBuffer | SharedArrayBuffer | ArrayBufferView) : void;
  }

  class OptionsObject {}

  class EndpointWrap {
    state: ArrayBuffer;
    stats: BigUint64Array;
    listen(options: OptionsObject, context: SecureContext) : void;
    waitForPendingCallbacks() : void;
    createClientSession(
      address: SocketAddress,
      options: OptionsObject,
      context: SecureContext,
      sessionTicket?: ArrayBuffer | SharedArrayBuffer | ArrayBufferView,
      remoteTransportParams?: ArrayBuffer | SharedArrayBuffer | ArrayBufferView) : void;
    address() : SocketAddress;
    ref() : void;
    unref() : void;
  }

  class RandomConnectionIDStrategy {}

  class Stream {
    destroy() : void;
  }

  class Session {
    getRemoteAddress() : SocketAddress | void;
    getCertificate() : X509Certificate | void;
    getPeerCertificate() : X509Certificate | void;
    getEphemeralKey() : {
      type: string,
      size: number,
      name?: string,
    } | void;
    destroy() : void;
    gracefulClose() : void;
    silentClose() : void;
    updateKey() : void;
    attachToEndpoint(endpoint : EndpointWrap) : void;
    detachFromEndpoint() : void;
    onClientHelloDone(context? : SecureContext) : void;
    onOCSPDone(response? : ArrayBuffer | SharedArrayBuffer | ArrayBufferView) : void;
    openStream(unidirectional? : boolean) : Stream | void;
  }

  interface Callbacks {
    onEndpointDone() : void,
    onEndpointError() : void,
    onSessionNew() : void,
    onSessionClientHello() : void,
    onSessionClose() : void,
    onSessionDatagram() : void,
    onSessionHandshake() : void,
    onSessionOcspRequest() : void,
    onSessionOcspResponse() : void,
    onSessionTicket() : void,
    onSessionVersionNegotiation() : void,
    onStreamClose() : void,
    onStreamCreated() : void,
    onStreamReset() : void,
    onStreamHeaders() : void,
    onStreamBlocked() : void,
    onStreamTrailers() : void,
  }

  function createClientSecureContext() : SecureContext;

  function createServerSecureContext() : SecureContext;

  function createEndpoint(config : ConfigObject) : EndpointWrap;

  function initializeCallbacks(callbacks : Callbacks) : void;
}

declare function InternalBinding(binding: 'quic'): {
  createClientSecureContext: typeof InternalQuicBinding.createClientSecureContext,
  createServerSecureContext: typeof InternalQuicBinding.createServerSecureContext,
  createEndpoint: typeof InternalQuicBinding.createEndpoint,
  initializeCallbacks: typeof InternalQuicBinding.initializeCallbacks,
  ConfigObject: typeof InternalQuicBinding.ConfigObject,
  OptionsObject: typeof InternalQuicBinding.OptionsObject,
  RandomConnectionIDStrategy: typeof InternalQuicBinding.RandomConnectionIDStrategy,
  JSQuicBufferConsumer: typeof InternalQuicBinding.JSQuicBufferConsumer,
  ArrayBufferViewSource: typeof InternalQuicBinding.ArrayBufferViewSource,
  StreamSource: typeof InternalQuicBinding.StreamSource,
  StreamBaseSource: typeof InternalQuicBinding.StreamBaseSource,
}
