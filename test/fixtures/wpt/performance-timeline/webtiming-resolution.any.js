function testTimeResolution(highResTimeFunc, funcString) {
    test(() => {
        const t0 = highResTimeFunc();
        let t1 = highResTimeFunc();
        while (t0 == t1) {
            t1 = highResTimeFunc();
        }
        const epsilon = 1e-5;
        assert_greater_than_equal(t1 - t0, 0.005 - epsilon, 'The second ' + funcString + ' should be much greater than the first');
    }, 'Verifies the resolution of ' + funcString + ' is at least 5 microseconds.');
}

function timeByPerformanceNow() {
    return performance.now();
}

function timeByUserTiming() {
    performance.mark('timer');
    const time = performance.getEntriesByName('timer')[0].startTime;
    performance.clearMarks('timer');
    return time;
}

testTimeResolution(timeByPerformanceNow, 'performance.now()');
testTimeResolution(timeByUserTiming, 'entry.startTime');
