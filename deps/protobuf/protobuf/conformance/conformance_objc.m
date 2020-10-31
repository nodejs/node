// Protocol Buffers - Google's data interchange format
// Copyright 2015 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import <Foundation/Foundation.h>

#import "Conformance.pbobjc.h"
#import "google/protobuf/TestMessagesProto2.pbobjc.h"
#import "google/protobuf/TestMessagesProto3.pbobjc.h"

static void Die(NSString *format, ...) __dead2;

static BOOL verbose = NO;
static int32_t testCount = 0;

static void Die(NSString *format, ...) {
  va_list args;
  va_start(args, format);
  NSString *msg = [[NSString alloc] initWithFormat:format arguments:args];
  NSLog(@"%@", msg);
  va_end(args);
  [msg release];
  exit(66);
}

static NSData *CheckedReadDataOfLength(NSFileHandle *handle, NSUInteger numBytes) {
  NSData *data = [handle readDataOfLength:numBytes];
  NSUInteger dataLen = data.length;
  if (dataLen == 0) {
    return nil;  // EOF.
  }
  if (dataLen != numBytes) {
    Die(@"Failed to read the request length (%d), only got: %@",
        numBytes, data);
  }
  return data;
}

static ConformanceResponse *DoTest(ConformanceRequest *request) {
  ConformanceResponse *response = [ConformanceResponse message];
  GPBMessage *testMessage = nil;

  switch (request.payloadOneOfCase) {
    case ConformanceRequest_Payload_OneOfCase_GPBUnsetOneOfCase:
      Die(@"Request didn't have a payload: %@", request);
      break;

    case ConformanceRequest_Payload_OneOfCase_ProtobufPayload: {
      Class msgClass = nil;
      if ([request.messageType isEqual:@"protobuf_test_messages.proto3.TestAllTypesProto3"]) {
        msgClass = [Proto3TestAllTypesProto3 class];
      } else if ([request.messageType isEqual:@"protobuf_test_messages.proto2.TestAllTypesProto2"]) {
        msgClass = [TestAllTypesProto2 class];
      } else {
        Die(@"Protobuf request had an unknown message_type: %@", request.messageType);
      }
      NSError *error = nil;
      testMessage = [msgClass parseFromData:request.protobufPayload error:&error];
      if (!testMessage) {
        response.parseError =
            [NSString stringWithFormat:@"Parse error: %@", error];
      }
      break;
    }

    case ConformanceRequest_Payload_OneOfCase_JsonPayload:
      response.skipped = @"ObjC doesn't support parsing JSON";
      break;

    case ConformanceRequest_Payload_OneOfCase_JspbPayload:
      response.skipped =
          @"ConformanceRequest had a jspb_payload ConformanceRequest.payload;"
          " those aren't supposed to happen with opensource.";
      break;

    case ConformanceRequest_Payload_OneOfCase_TextPayload:
      response.skipped = @"ObjC doesn't support parsing TextFormat";
      break;
  }

  if (testMessage) {
    switch (request.requestedOutputFormat) {
      case WireFormat_GPBUnrecognizedEnumeratorValue:
      case WireFormat_Unspecified:
        Die(@"Unrecognized/unspecified output format: %@", request);
        break;

      case WireFormat_Protobuf:
        response.protobufPayload = testMessage.data;
        if (!response.protobufPayload) {
          response.serializeError =
            [NSString stringWithFormat:@"Failed to make data from: %@", testMessage];
        }
        break;

      case WireFormat_Json:
        response.skipped = @"ObjC doesn't support generating JSON";
        break;

      case WireFormat_Jspb:
        response.skipped =
            @"ConformanceRequest had a requested_output_format of JSPB WireFormat; that"
            " isn't supposed to happen with opensource.";
        break;

      case WireFormat_TextFormat:
        // ObjC only has partial objc generation, so don't attempt any tests that need
        // support.
        response.skipped = @"ObjC doesn't support generating TextFormat";
        break;
    }
  }

  return response;
}

static uint32_t UInt32FromLittleEndianData(NSData *data) {
  if (data.length != sizeof(uint32_t)) {
    Die(@"Data not the right size for uint32_t: %@", data);
  }
  uint32_t value;
  memcpy(&value, data.bytes, sizeof(uint32_t));
  return CFSwapInt32LittleToHost(value);
}

static NSData *UInt32ToLittleEndianData(uint32_t num) {
  uint32_t value = CFSwapInt32HostToLittle(num);
  return [NSData dataWithBytes:&value length:sizeof(uint32_t)];
}

static BOOL DoTestIo(NSFileHandle *input, NSFileHandle *output) {
  // See conformance_test_runner.cc for the wire format.
  NSData *data = CheckedReadDataOfLength(input, sizeof(uint32_t));
  if (!data) {
    // EOF.
    return NO;
  }
  uint32_t numBytes = UInt32FromLittleEndianData(data);
  data = CheckedReadDataOfLength(input, numBytes);
  if (!data) {
    Die(@"Failed to read request");
  }

  NSError *error = nil;
  ConformanceRequest *request = [ConformanceRequest parseFromData:data
                                                            error:&error];
  if (!request) {
    Die(@"Failed to parse the message data: %@", error);
  }

  ConformanceResponse *response = DoTest(request);
  if (!response) {
    Die(@"Failed to make a reply from %@", request);
  }

  data = response.data;
  [output writeData:UInt32ToLittleEndianData((int32_t)data.length)];
  [output writeData:data];

  if (verbose) {
    NSLog(@"Request: %@", request);
    NSLog(@"Response: %@", response);
  }

  ++testCount;
  return YES;
}

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    NSFileHandle *input = [[NSFileHandle fileHandleWithStandardInput] retain];
    NSFileHandle *output = [[NSFileHandle fileHandleWithStandardOutput] retain];

    BOOL notDone = YES;
    while (notDone) {
      @autoreleasepool {
        notDone = DoTestIo(input, output);
      }
    }

    NSLog(@"Received EOF from test runner after %d tests, exiting.", testCount);
  }
  return 0;
}
