// Check to see if the worker handles pending events. Messages after close() will not be sent to the parent page, so we use exceptions instead to report failures after close().
onmessage = function(evt)
{
    if (evt.data == "closeWithPendingEvents") {
        // Set a timer to generate an event - minimum timeout is 1ms.
        setTimeout(function() {
                postMessage("pending event processed");
                throw "should not be executed";
            }, 1);
        var start = new Date().getTime();
        // Loop for 10 ms so the timer is ready to fire
        while (new Date().getTime() - start < 100)
            ;
        // Now close - timer should not fire
        close();
    } else if (evt.data == "typeofClose") {
        postMessage("typeof close: " + (typeof close));
    } else if (evt.data == "close") {
        close();
        postMessage("Should be delivered");
    } else if (evt.data == "ping") {
        postMessage("pong");
    } else if (evt.data == "throw") {
        throw "should never be executed";
    } else if (evt.data == "closeWithError") {
        close();
        nonExistentFunction();  // Undefined function - throws exception
    } else if (evt.data == "close_post_loop") {
        close();
        postMessage("closed");
        while(true) {} // Should loop forever.
    } else if (evt.data == "take_port") {
        messagePort = evt.ports[0];
        messagePort.onmessage = function(event) {
            close();
            postMessage("echo_" + event.data);
        }
    } else if (evt.data == "start_port") {
        messagePort.start();
    } else {
        postMessage("FAIL: Unknown message type: " + evt.data);
    }
}
