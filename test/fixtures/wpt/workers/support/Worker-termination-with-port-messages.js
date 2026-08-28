function echo(evt)
{
    evt.target.postMessage(evt.data);
}

onmessage = function(evt)
{
    evt.ports[0].onmessage = echo;
    evt.ports[0].start();
}
