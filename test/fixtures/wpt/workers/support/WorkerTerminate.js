onmessage = function(evt)
{
    for (var i=0; true; i++)
    {
        if (i%1000 == 0)
        {
            postMessage(i);
        }
    }
}
