var result = "Fail";

try
{
    importScripts("NonExistentFile.js");
}
catch(ex)
{
    if (ex.code != null && ex.code == ex.NETWORK_ERR)
    {
        result = "Pass";
    }
}

postMessage(result);