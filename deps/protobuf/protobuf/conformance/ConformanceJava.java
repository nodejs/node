import com.google.protobuf.AbstractMessage;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedInputStream;
import com.google.protobuf.ExtensionRegistry;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protobuf.Parser;
import com.google.protobuf.TextFormat;
import com.google.protobuf.conformance.Conformance;
import com.google.protobuf.util.JsonFormat;
import com.google.protobuf.util.JsonFormat.TypeRegistry;
import com.google.protobuf_test_messages.proto2.TestMessagesProto2;
import com.google.protobuf_test_messages.proto2.TestMessagesProto2.TestAllTypesProto2;
import com.google.protobuf_test_messages.proto3.TestMessagesProto3;
import com.google.protobuf_test_messages.proto3.TestMessagesProto3.TestAllTypesProto3;
import java.nio.ByteBuffer;
import java.util.ArrayList;

class ConformanceJava {
  private int testCount = 0;
  private TypeRegistry typeRegistry;

  private boolean readFromStdin(byte[] buf, int len) throws Exception {
    int ofs = 0;
    while (len > 0) {
      int read = System.in.read(buf, ofs, len);
      if (read == -1) {
        return false;  // EOF
      }
      ofs += read;
      len -= read;
    }

    return true;
  }

  private void writeToStdout(byte[] buf) throws Exception {
    System.out.write(buf);
  }

  // Returns -1 on EOF (the actual values will always be positive).
  private int readLittleEndianIntFromStdin() throws Exception {
    byte[] buf = new byte[4];
    if (!readFromStdin(buf, 4)) {
      return -1;
    }
    return (buf[0] & 0xff)
        | ((buf[1] & 0xff) << 8)
        | ((buf[2] & 0xff) << 16)
        | ((buf[3] & 0xff) << 24);
  }

  private void writeLittleEndianIntToStdout(int val) throws Exception {
    byte[] buf = new byte[4];
    buf[0] = (byte)val;
    buf[1] = (byte)(val >> 8);
    buf[2] = (byte)(val >> 16);
    buf[3] = (byte)(val >> 24);
    writeToStdout(buf);
  }

  private enum BinaryDecoderType {
    BTYE_STRING_DECODER,
    BYTE_ARRAY_DECODER,
    ARRAY_BYTE_BUFFER_DECODER,
    READONLY_ARRAY_BYTE_BUFFER_DECODER,
    DIRECT_BYTE_BUFFER_DECODER,
    READONLY_DIRECT_BYTE_BUFFER_DECODER,
    INPUT_STREAM_DECODER;
  }

  private static class BinaryDecoder <MessageType extends AbstractMessage> {
    public MessageType decode (ByteString bytes, BinaryDecoderType type,
        Parser <MessageType> parser, ExtensionRegistry extensions)
      throws InvalidProtocolBufferException {
      switch (type) {
        case BTYE_STRING_DECODER:
          return parser.parseFrom(bytes, extensions);
        case BYTE_ARRAY_DECODER:
          return parser.parseFrom(bytes.toByteArray(), extensions);
        case ARRAY_BYTE_BUFFER_DECODER: {
          ByteBuffer buffer = ByteBuffer.allocate(bytes.size());
          bytes.copyTo(buffer);
          buffer.flip();
          try {
            return parser.parseFrom(CodedInputStream.newInstance(buffer), extensions);
          } catch (InvalidProtocolBufferException e) {
            throw e;
          }
        }
        case READONLY_ARRAY_BYTE_BUFFER_DECODER: {
          try {
            return parser.parseFrom(
                CodedInputStream.newInstance(bytes.asReadOnlyByteBuffer()), extensions);
          } catch (InvalidProtocolBufferException e) {
            throw e;
          }
        }
        case DIRECT_BYTE_BUFFER_DECODER: {
          ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.size());
          bytes.copyTo(buffer);
          buffer.flip();
          try {
            return parser.parseFrom(CodedInputStream.newInstance(buffer), extensions);
          } catch (InvalidProtocolBufferException e) {
            throw e;
          }
        }
        case READONLY_DIRECT_BYTE_BUFFER_DECODER: {
          ByteBuffer buffer = ByteBuffer.allocateDirect(bytes.size());
          bytes.copyTo(buffer);
          buffer.flip();
          try {
            return parser.parseFrom(
                CodedInputStream.newInstance(buffer.asReadOnlyBuffer()), extensions);
          } catch (InvalidProtocolBufferException e) {
            throw e;
          }
        }
        case INPUT_STREAM_DECODER: {
          try {
            return parser.parseFrom(bytes.newInput(), extensions);
          } catch (InvalidProtocolBufferException e) {
            throw e;
          }
        }
        default :
          return null;
      }
    }
  }

  private <MessageType extends AbstractMessage> MessageType parseBinary(
      ByteString bytes, Parser <MessageType> parser, ExtensionRegistry extensions)
      throws InvalidProtocolBufferException {
    ArrayList <MessageType> messages = new ArrayList <MessageType> ();
    ArrayList <InvalidProtocolBufferException> exceptions =
        new ArrayList <InvalidProtocolBufferException>();

    for (int i = 0; i < BinaryDecoderType.values().length; i++) {
      messages.add(null);
      exceptions.add(null);
    }
    BinaryDecoder <MessageType> decoder = new BinaryDecoder <MessageType> ();

    boolean hasMessage = false;
    boolean hasException = false;
    for (int i = 0; i < BinaryDecoderType.values().length; ++i) {
      try {
        //= BinaryDecoderType.values()[i].parseProto3(bytes);
        messages.set(i, decoder.decode(bytes, BinaryDecoderType.values()[i], parser, extensions));
        hasMessage = true;
      } catch (InvalidProtocolBufferException e) {
        exceptions.set(i, e);
        hasException = true;
      }
    }

    if (hasMessage && hasException) {
      StringBuilder sb =
          new StringBuilder("Binary decoders disagreed on whether the payload was valid.\n");
      for (int i = 0; i < BinaryDecoderType.values().length; ++i) {
        sb.append(BinaryDecoderType.values()[i].name());
        if (messages.get(i) != null) {
          sb.append(" accepted the payload.\n");
        } else {
          sb.append(" rejected the payload.\n");
        }
      }
      throw new RuntimeException(sb.toString());
    }

    if (hasException) {
      // We do not check if exceptions are equal. Different implementations may return different
      // exception messages. Throw an arbitrary one out instead.
      throw exceptions.get(0);
    }

    // Fast path comparing all the messages with the first message, assuming equality being
    // symmetric and transitive.
    boolean allEqual = true;
    for (int i = 1; i < messages.size(); ++i) {
      if (!messages.get(0).equals(messages.get(i))) {
        allEqual = false;
        break;
      }
    }

    // Slow path: compare and find out all unequal pairs.
    if (!allEqual) {
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < messages.size() - 1; ++i) {
        for (int j = i + 1; j < messages.size(); ++j) {
          if (!messages.get(i).equals(messages.get(j))) {
            sb.append(BinaryDecoderType.values()[i].name())
                .append(" and ")
                .append(BinaryDecoderType.values()[j].name())
                .append(" parsed the payload differently.\n");
          }
        }
      }
      throw new RuntimeException(sb.toString());
    }

    return messages.get(0);
  }

  private Conformance.ConformanceResponse doTest(Conformance.ConformanceRequest request) {
    com.google.protobuf.AbstractMessage testMessage;
    boolean isProto3 = request.getMessageType().equals("protobuf_test_messages.proto3.TestAllTypesProto3");
    boolean isProto2 = request.getMessageType().equals("protobuf_test_messages.proto2.TestAllTypesProto2");

    switch (request.getPayloadCase()) {
      case PROTOBUF_PAYLOAD: {
        if (isProto3) {
          try {
            ExtensionRegistry extensions = ExtensionRegistry.newInstance();
            TestMessagesProto3.registerAllExtensions(extensions);
            testMessage = parseBinary(request.getProtobufPayload(), TestAllTypesProto3.parser(), extensions);
          } catch (InvalidProtocolBufferException e) {
            return Conformance.ConformanceResponse.newBuilder().setParseError(e.getMessage()).build();
          }
        } else if (isProto2) {
          try {
            ExtensionRegistry extensions = ExtensionRegistry.newInstance();
            TestMessagesProto2.registerAllExtensions(extensions);
            testMessage = parseBinary(request.getProtobufPayload(), TestAllTypesProto2.parser(), extensions);
          } catch (InvalidProtocolBufferException e) {
            return Conformance.ConformanceResponse.newBuilder().setParseError(e.getMessage()).build();
          }
        } else {
          throw new RuntimeException("Protobuf request doesn't have specific payload type.");
        }
        break;
      }
      case JSON_PAYLOAD: {
        try {
          TestMessagesProto3.TestAllTypesProto3.Builder builder =
              TestMessagesProto3.TestAllTypesProto3.newBuilder();
          JsonFormat.Parser parser = JsonFormat.parser().usingTypeRegistry(typeRegistry);
          if (request.getTestCategory()
              == Conformance.TestCategory.JSON_IGNORE_UNKNOWN_PARSING_TEST) {
            parser = parser.ignoringUnknownFields();
          }
          parser.merge(request.getJsonPayload(), builder);
          testMessage = builder.build();
        } catch (InvalidProtocolBufferException e) {
          return Conformance.ConformanceResponse.newBuilder().setParseError(e.getMessage()).build();
        }
        break;
      }
      case TEXT_PAYLOAD: {
        if (isProto3) {
          try {
            TestMessagesProto3.TestAllTypesProto3.Builder builder =
                TestMessagesProto3.TestAllTypesProto3.newBuilder();
            TextFormat.merge(request.getTextPayload(), builder);
            testMessage = builder.build();
          } catch (TextFormat.ParseException e) {
              return Conformance.ConformanceResponse.newBuilder()
                  .setParseError(e.getMessage())
                  .build();
          }
        } else if (isProto2) {
          try {
            TestMessagesProto2.TestAllTypesProto2.Builder builder =
                TestMessagesProto2.TestAllTypesProto2.newBuilder();
            TextFormat.merge(request.getTextPayload(), builder);
            testMessage = builder.build();
          } catch (TextFormat.ParseException e) {
              return Conformance.ConformanceResponse.newBuilder()
                  .setParseError(e.getMessage())
                  .build();
          }
        } else {
          throw new RuntimeException("Protobuf request doesn't have specific payload type.");
        }
        break;
      }
      case PAYLOAD_NOT_SET: {
        throw new RuntimeException("Request didn't have payload.");
      }

      default: {
        throw new RuntimeException("Unexpected payload case.");
      }
    }

    switch (request.getRequestedOutputFormat()) {
      case UNSPECIFIED:
        throw new RuntimeException("Unspecified output format.");

      case PROTOBUF: {
        ByteString MessageString = testMessage.toByteString();
        return Conformance.ConformanceResponse.newBuilder().setProtobufPayload(MessageString).build();
      }

      case JSON:
        try {
          return Conformance.ConformanceResponse.newBuilder().setJsonPayload(
              JsonFormat.printer().usingTypeRegistry(typeRegistry).print(testMessage)).build();
        } catch (InvalidProtocolBufferException | IllegalArgumentException e) {
          return Conformance.ConformanceResponse.newBuilder().setSerializeError(
              e.getMessage()).build();
        }

      case TEXT_FORMAT:
        return Conformance.ConformanceResponse.newBuilder().setTextPayload(
            TextFormat.printToString(testMessage)).build();

      default: {
        throw new RuntimeException("Unexpected request output.");
      }
    }
  }

  private boolean doTestIo() throws Exception {
    int bytes = readLittleEndianIntFromStdin();

    if (bytes == -1) {
      return false;  // EOF
    }

    byte[] serializedInput = new byte[bytes];

    if (!readFromStdin(serializedInput, bytes)) {
      throw new RuntimeException("Unexpected EOF from test program.");
    }

    Conformance.ConformanceRequest request =
        Conformance.ConformanceRequest.parseFrom(serializedInput);
    Conformance.ConformanceResponse response = doTest(request);
    byte[] serializedOutput = response.toByteArray();

    writeLittleEndianIntToStdout(serializedOutput.length);
    writeToStdout(serializedOutput);

    return true;
  }

  public void run() throws Exception {
    typeRegistry = TypeRegistry.newBuilder().add(
        TestMessagesProto3.TestAllTypesProto3.getDescriptor()).build();
    while (doTestIo()) {
      this.testCount++;
    }

    System.err.println("ConformanceJava: received EOF from test runner after " +
        this.testCount + " tests");
  }

  public static void main(String[] args) throws Exception {
    new ConformanceJava().run();
  }
}
