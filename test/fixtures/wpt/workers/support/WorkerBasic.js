var result = "Fail";

onmessage = function(evt)
{
    result = "Pass";
    postMessage(result);
}