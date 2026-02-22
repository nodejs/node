function checkResponseURL(fetchedURL, expectedURL)
{
    promise_test(function() {
        return fetch(fetchedURL).then(function(response) {
            assert_equals(response.url, expectedURL);
        });
    }, "Testing response url getter with " +fetchedURL);
}

var baseURL = "http://{{host}}:{{ports[http][0]}}";
checkResponseURL(baseURL + "/ada", baseURL + "/ada");
checkResponseURL(baseURL + "/#", baseURL + "/");
checkResponseURL(baseURL + "/#ada", baseURL + "/");
checkResponseURL(baseURL + "#ada", baseURL + "/");

done();
