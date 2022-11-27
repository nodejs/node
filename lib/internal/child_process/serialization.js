const { createChildProcessSerializer } = require('./helper.js');

const advanced = {
  initMessageChannel(channel) {
    channel[kMessageBuffer] = [];
    channel[kMessageBufferSize] = 0;
    channel.buffering = false;
  },

  *parseChannelMessages(channel, readData) {
    if (readData.length === 0) return;

    ArrayPrototypePush(channel[kMessageBuffer], readData);
    channel[kMessageBufferSize] += readData.length;

    // Index 0 should always be present because we just pushed data into it.
    let messageBufferHead = channel[kMessageBuffer][0];
    while (messageBufferHead.length >= 4) {
      // We call `readUInt32BE` manually here, because this is faster than first converting
      // it to a buffer and using `readUInt32BE` on that.
      const fullMessageSize = (
        messageBufferHead[0] << 24 |
        messageBufferHead[1] << 16 |
        messageBufferHead[2] << 8 |
        messageBufferHead[3]
      ) + 4;

      if (channel[kMessageBufferSize] < fullMessageSize) break;

      const concatenatedBuffer =
        channel[kMessageBuffer].length === 1
          ? channel[kMessageBuffer][0]
          : Buffer.concat(channel[kMessageBuffer], channel[kMessageBufferSize]);

      const deserializer = new ChildProcessDeserializer(
        TypedArrayPrototypeSubarray(concatenatedBuffer, 4, fullMessageSize)
      );

      messageBufferHead =
        TypedArrayPrototypeSubarray(concatenatedBuffer, fullMessageSize);
      channel[kMessageBufferSize] = messageBufferHead.length;
      channel[kMessageBuffer] =
        channel[kMessageBufferSize] !== 0 ? [messageBufferHead] : [];

      deserializer.readHeader();
      yield deserializer.readValue();
    }

    channel.buffering = channel[kMessageBufferSize] > 0;
  },

  writeChannelMessage(channel, req, message, handle) {
    const ser = createChildProcessSerializer();
    // Add 4 bytes, to later populate with message length
    ser.writeRawBytes(Buffer.allocUnsafe(4));
    ser.writeHeader();
    ser.writeValue(message);

    const serializedMessage = ser.releaseBuffer();
    const serializedMessageLength = serializedMessage.length - 4;

    serializedMessage.set([
      serializedMessageLength >> 24 & 0xFF,
      serializedMessageLength >> 16 & 0xFF,
      serializedMessageLength >> 8 & 0xFF,
      serializedMessageLength & 0xFF,
    ], 0);

    const result = channel.writeBuffer(req, serializedMessage, handle);

    // Mirror what stream_base_commons.js does for Buffer retention.
    if (streamBaseState[kLastWriteWasAsync]) req.buffer = serializedMessage;

    return result;
  },
};
