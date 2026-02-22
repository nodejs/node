promise_test(async function () {
    const req1 = new Request("https://example.com/", {
        body: "req1",
        method: "POST",
    });

    const text1 = await req1.text();
    assert_equals(
        text1,
        "req1",
        "The body of the first request should be 'req1'."
    );

    const req2 = new Request(req1, { body: "req2" });
    const bodyText = await req2.text();
    assert_equals(
        bodyText,
        "req2",
        "The body of the second request should be overridden to 'req2'."
    );

}, "Check that the body of a new request can be overridden when created from an existing Request object");

promise_test(async function () {
    const req1 = new Request("https://example.com/", {
        body: "req1",
        method: "POST",
    });

    const req2 = new Request("https://example.com/", req1);
    const bodyText = await req2.text();
    assert_equals(
        bodyText,
        "req1",
        "The body of the second request should be the same as the first."
    );
}, "Check that the body of a new request can be duplicated from an existing Request object");
