onmessage = function(evt)
{
    if (evt.data == "ping")
        postMessage("pong");
    else if (evt.data == "freeze")
        while (1) {}
    else if (evt.data == "close")
        close();
    else if (/eval.+/.test(evt.data)) {
        try {
            postMessage(evt.data.substr(5) + ": " + eval(evt.data.substr(5)));
        } catch (ex) {
            postMessage(evt.data.substr(5) + ": " + ex);
        }
    }
}
