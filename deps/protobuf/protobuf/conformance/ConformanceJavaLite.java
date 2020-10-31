
import com.google.protobuf.conformance.Conformance;
import com.google.protobuf.InvalidProtocolBufferException;

class ConformanceJavaLite {
  private int testCount = 0;

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

  private Conformance.ConformanceResponse doTest(Conformance.ConformanceRequest request) {
    Conformance.TestAllTypes testMessage;

    switch (request.getPayloadCase()) {
      case PROTOBUF_PAYLOAD: {
        try {
          testMessage = Conformance.TestAllTypes.parseFrom(request.getProtobufPayload());
        } catch (InvalidProtocolBufferException e) {
          return Conformance.ConformanceResponse.newBuilder().setParseError(e.getMessage()).build();
        }
        break;
      }
      case JSON_PAYLOAD: {
        return Conformance.ConformanceResponse.newBuilder().setSkipped(
            "Lite runtime does not support JSON format.").build();
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

      case PROTOBUF:
        return Conformance.ConformanceResponse.newBuilder().setProtobufPayload(testMessage.toByteString()).build();

      case JSON:
        return Conformance.ConformanceResponse.newBuilder().setSkipped(
            "Lite runtime does not support JSON format.").build();

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
    while (doTestIo()) {
      this.testCount++;
    }

    System.err.println("ConformanceJavaLite: received EOF from test runner after " +
        this.testCount + " tests");
  }

  public static void main(String[] args) throws Exception {
    new ConformanceJavaLite().run();
  }
}
