'use strict';

// TODO(@jasnell) Temporarily ignoring c8 covrerage for this file while tests
// are still being developed.
/* c8 ignore start */

const dc = require('diagnostics_channel');

const onEndpointCreatedChannel = dc.channel('quic.endpoint.created');
const onEndpointListeningChannel = dc.channel('quic.endpoint.listen');
const onEndpointClosingChannel = dc.channel('quic.endpoint.closing');
const onEndpointClosedChannel = dc.channel('quic.endpoint.closed');
const onEndpointErrorChannel = dc.channel('quic.endpoint.error');
const onEndpointBusyChangeChannel = dc.channel('quic.endpoint.busy.change');
const onEndpointClientSessionChannel = dc.channel('quic.session.created.client');
const onEndpointServerSessionChannel = dc.channel('quic.session.created.server');
const onSessionOpenStreamChannel = dc.channel('quic.session.open.stream');
const onSessionReceivedStreamChannel = dc.channel('quic.session.received.stream');
const onSessionSendDatagramChannel = dc.channel('quic.session.send.datagram');
const onSessionUpdateKeyChannel = dc.channel('quic.session.update.key');
const onSessionClosingChannel = dc.channel('quic.session.closing');
const onSessionClosedChannel = dc.channel('quic.session.closed');
const onSessionReceiveDatagramChannel = dc.channel('quic.session.receive.datagram');
const onSessionReceiveDatagramStatusChannel = dc.channel('quic.session.receive.datagram.status');
const onSessionPathValidationChannel = dc.channel('quic.session.path.validation');
const onSessionNewTokenChannel = dc.channel('quic.session.new.token');
const onSessionTicketChannel = dc.channel('quic.session.ticket');
const onSessionVersionNegotiationChannel = dc.channel('quic.session.version.negotiation');
const onSessionOriginChannel = dc.channel('quic.session.receive.origin');
const onSessionHandshakeChannel = dc.channel('quic.session.handshake');
const onSessionGoawayChannel = dc.channel('quic.session.goaway');
const onSessionEarlyRejectedChannel = dc.channel('quic.session.early.rejected');
const onStreamClosedChannel = dc.channel('quic.stream.closed');
const onStreamHeadersChannel = dc.channel('quic.stream.headers');
const onStreamTrailersChannel = dc.channel('quic.stream.trailers');
const onStreamInfoChannel = dc.channel('quic.stream.info');
const onStreamResetChannel = dc.channel('quic.stream.reset');
const onStreamBlockedChannel = dc.channel('quic.stream.blocked');
const onSessionErrorChannel = dc.channel('quic.session.error');
const onEndpointConnectChannel = dc.channel('quic.endpoint.connect');

module.exports = {
  onEndpointCreatedChannel,
  onEndpointListeningChannel,
  onEndpointClosingChannel,
  onEndpointClosedChannel,
  onEndpointErrorChannel,
  onEndpointBusyChangeChannel,
  onEndpointClientSessionChannel,
  onEndpointServerSessionChannel,
  onSessionOpenStreamChannel,
  onSessionReceivedStreamChannel,
  onSessionSendDatagramChannel,
  onSessionUpdateKeyChannel,
  onSessionClosingChannel,
  onSessionClosedChannel,
  onSessionReceiveDatagramChannel,
  onSessionReceiveDatagramStatusChannel,
  onSessionPathValidationChannel,
  onSessionNewTokenChannel,
  onSessionTicketChannel,
  onSessionVersionNegotiationChannel,
  onSessionOriginChannel,
  onSessionHandshakeChannel,
  onSessionGoawayChannel,
  onSessionEarlyRejectedChannel,
  onStreamClosedChannel,
  onStreamHeadersChannel,
  onStreamTrailersChannel,
  onStreamInfoChannel,
  onStreamResetChannel,
  onStreamBlockedChannel,
  onSessionErrorChannel,
  onEndpointConnectChannel,
};

/* c8 ignore stop */
