try
{
    importScripts("WorkerBasic.js");
}
catch(ex)
{
    result = "Fail";
    postMessage(result);
}
