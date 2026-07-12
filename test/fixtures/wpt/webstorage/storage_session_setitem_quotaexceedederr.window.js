test(t => {
    sessionStorage.clear();

    var index = 0;
    var key = "name";
    var val = "x".repeat(1024);

    t.add_cleanup(() => {
        sessionStorage.clear();
    });

    assert_throws_quotaexceedederror(() => {
        while (true) {
            index++;
            sessionStorage.setItem("" + key + index, "" + val + index);
        }
    }, null, null);
}, "Throws QuotaExceededError when the quota has been exceeded");
