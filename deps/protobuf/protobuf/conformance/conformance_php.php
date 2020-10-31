<?php

require_once("Conformance/WireFormat.php");
require_once("Conformance/ConformanceResponse.php");
require_once("Conformance/ConformanceRequest.php");
require_once("Conformance/FailureSet.php");
require_once("Conformance/JspbEncodingConfig.php");
require_once("Conformance/TestCategory.php");
require_once("Protobuf_test_messages/Proto3/ForeignMessage.php");
require_once("Protobuf_test_messages/Proto3/ForeignEnum.php");
require_once("Protobuf_test_messages/Proto3/TestAllTypesProto3.php");
require_once("Protobuf_test_messages/Proto3/TestAllTypesProto3/AliasedEnum.php");
require_once("Protobuf_test_messages/Proto3/TestAllTypesProto3/NestedMessage.php");
require_once("Protobuf_test_messages/Proto3/TestAllTypesProto3/NestedEnum.php");

require_once("GPBMetadata/Conformance.php");
require_once("GPBMetadata/Google/Protobuf/TestMessagesProto3.php");

use  \Conformance\TestCategory;
use  \Conformance\WireFormat;

if (!ini_get("date.timezone")) {
  ini_set("date.timezone", "UTC");
}

$test_count = 0;

function doTest($request)
{
    $test_message = new \Protobuf_test_messages\Proto3\TestAllTypesProto3();
    $response = new \Conformance\ConformanceResponse();
    if ($request->getPayload() == "protobuf_payload") {
      if ($request->getMessageType() == "conformance.FailureSet") {
        $response->setProtobufPayload("");
        return $response;
      } elseif ($request->getMessageType() == "protobuf_test_messages.proto3.TestAllTypesProto3") {
        try {
          $test_message->mergeFromString($request->getProtobufPayload());
        } catch (Exception $e) {
          $response->setParseError($e->getMessage());
          return $response;
        }
      } elseif ($request->getMessageType() == "protobuf_test_messages.proto2.TestAllTypesProto2") {
        $response->setSkipped("PHP doesn't support proto2");
        return $response;
      } else {
        trigger_error("Protobuf request doesn't have specific payload type", E_USER_ERROR);
      }
    } elseif ($request->getPayload() == "json_payload") {
      $ignore_json_unknown =
          ($request->getTestCategory() ==
              TestCategory::JSON_IGNORE_UNKNOWN_PARSING_TEST);
      try {
          $test_message->mergeFromJsonString($request->getJsonPayload(),
                                             $ignore_json_unknown);
      } catch (Exception $e) {
          $response->setParseError($e->getMessage());
          return $response;
      }
	} elseif ($request->getPayload() == "text_payload") {
		$response->setSkipped("PHP doesn't support text format yet");
        return $response;
	} else {
      trigger_error("Request didn't have payload.", E_USER_ERROR);
    }

    if ($request->getRequestedOutputFormat() == WireFormat::UNSPECIFIED) {
      trigger_error("Unspecified output format.", E_USER_ERROR);
    } elseif ($request->getRequestedOutputFormat() == WireFormat::PROTOBUF) {
      $response->setProtobufPayload($test_message->serializeToString());
    } elseif ($request->getRequestedOutputFormat() == WireFormat::JSON) {
      try {
          $response->setJsonPayload($test_message->serializeToJsonString());
      } catch (Exception $e) {
          $response->setSerializeError($e->getMessage());
          return $response;
      }
    }

    return $response;
}

function doTestIO()
{
    $length_bytes = fread(STDIN, 4);
    if (strlen($length_bytes) == 0) {
      return false;   # EOF
    } elseif (strlen($length_bytes) != 4) {
      fwrite(STDERR, "I/O error\n");
      return false;
    }

    $length = unpack("V", $length_bytes)[1];
    $serialized_request = fread(STDIN, $length);
    if (strlen($serialized_request) != $length) {
      trigger_error("I/O error", E_USER_ERROR);
    }

    $request = new \Conformance\ConformanceRequest();
    $request->mergeFromString($serialized_request);

    $response = doTest($request);

    $serialized_response = $response->serializeToString();
    fwrite(STDOUT, pack("V", strlen($serialized_response)));
    fwrite(STDOUT, $serialized_response);

    $GLOBALS['test_count'] += 1;

    return true;
}

while(true){
  if (!doTestIO()) {
      fprintf(STDERR,
             "conformance_php: received EOF from test runner " +
             "after %d tests, exiting\n", $test_count);
      exit;
  }
}
