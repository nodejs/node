test(t => {
    localStorage.clear();

    var index = 0;
    var key = "name";
    var val = "x".repeat(1024);

    t.add_cleanup(() => {
        localStorage.clear();
    });

    assert_throws_quotaexceedederror(() => {
        while (true) {
            index++;
            localStorage.setItem("" + key + index, "" + val + index);
        }
    }, null, null);
}, "Throws QuotaExceededError when the quota has been exceeded");
