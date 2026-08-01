onmessage = function(event) {
  if (event.data == "noport") {
    if (event.ports && !event.ports.length)
      testPassed("event.ports is non-null and zero length when no port sent");
    else
      testFailed("event.ports is null or non-zero length when no port sent");
  } else if (event.data == "zero ports") {
    if (event.ports && !event.ports.length)
      testPassed("event.ports is non-null and zero length when empty array sent");
    else
      testFailed("event.ports is null or non-zero length when empty array sent");
  } else if (event.data == "two ports") {
    if (!event.ports) {
      testFailed("event.ports should be non-null when ports sent");
      return;
    }
    if (event.ports.length == 2)
      testPassed("event.ports contains two ports when two ports sent");
    else
      testFailed("event.ports contained " + event.ports.length + " when two ports sent");
  } else if (event.data == "failed ports") {
    if (event.ports.length == 2)
      testPassed("event.ports contains two ports when two ports re-sent after error");
    else
      testFailed("event.ports contained " + event.ports.length + " when two ports re-sent after error");
  } else if (event.data == "noargs") {
    try {
      postMessage();
      testFailed("postMessage() did not throw");
    } catch (e) {
      testPassed("postMessage() threw exception: " + e);
    }
  } else
    testFailed("Received unexpected message: " + event.data);
};

function testPassed(msg) {
  postMessage("PASS"+msg);
}

function testFailed(msg) {
  postMessage("FAIL"+msg);
}
