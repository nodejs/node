// META: script=resources/user-timing-helper.js

function cleanupPerformanceTimeline() {
    performance.clearMarks();
    performance.clearMeasures();
}

async_test(function (t) {
    this.add_cleanup(cleanupPerformanceTimeline);
    let measureEntries = [];
    const timeStamp1 = 784.4;
    const timeStamp2 = 1234.5;
    const timeStamp3 = 66.6;
    const timeStamp4 = 5566;
    const expectedEntries =
        [{ entryType: "measure", name: "measure1", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure2", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure3", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure4", detail: null },
        { entryType: "measure", name: "measure5", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure6", detail: null, startTime: timeStamp1 },
        { entryType: "measure", name: "measure7", detail: null, startTime: timeStamp1, duration: timeStamp2 - timeStamp1 },
        { entryType: "measure", name: "measure8", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure9", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure10", detail: null, startTime: timeStamp1 },
        { entryType: "measure", name: "measure11", detail: null, startTime: timeStamp3 },
        { entryType: "measure", name: "measure12", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure13", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure14", detail: null, startTime: timeStamp3, duration: timeStamp1 - timeStamp3 },
        { entryType: "measure", name: "measure15", detail: null, startTime: timeStamp1, duration: timeStamp2 - timeStamp1 },
        { entryType: "measure", name: "measure16", detail: null, startTime: timeStamp1 },
        { entryType: "measure", name: "measure17", detail: { customInfo: 159 }, startTime: timeStamp3, duration: timeStamp2 - timeStamp3 },
        { entryType: "measure", name: "measure18", detail: null, startTime: timeStamp1, duration: timeStamp2 - timeStamp1 },
        { entryType: "measure", name: "measure19", detail: null, startTime: timeStamp1, duration: timeStamp2 - timeStamp1 },
        { entryType: "measure", name: "measure20", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure21", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure22", detail: null, startTime: 0 },
        { entryType: "measure", name: "measure23", detail: null, startTime: 0 }];
    const observer = new PerformanceObserver(
        t.step_func(function (entryList, obs) {
          measureEntries =
            measureEntries.concat(entryList.getEntries());
          if (measureEntries.length >= expectedEntries.length) {
            checkEntries(measureEntries, expectedEntries);
            observer.disconnect();
            t.done();
          }
        })
      );
    observer.observe({ entryTypes: ["measure"] });
    self.performance.mark("mark1", { detail: { randomInfo: 3 }, startTime: timeStamp1 });
    self.performance.mark("mark2", { startTime: timeStamp2 });

    const returnedEntries = [];
    returnedEntries.push(self.performance.measure("measure1"));
    returnedEntries.push(self.performance.measure("measure2", undefined));
    returnedEntries.push(self.performance.measure("measure3", null));
    returnedEntries.push(self.performance.measure("measure4", 'mark1'));
    returnedEntries.push(
        self.performance.measure("measure5", null, 'mark1'));
    returnedEntries.push(
        self.performance.measure("measure6", 'mark1', undefined));
    returnedEntries.push(
        self.performance.measure("measure7", 'mark1', 'mark2'));
    returnedEntries.push(
        self.performance.measure("measure8", {}));
    returnedEntries.push(
        self.performance.measure("measure9", { start: undefined }));
    returnedEntries.push(
        self.performance.measure("measure10", { start: 'mark1' }));
    returnedEntries.push(
        self.performance.measure("measure11", { start: timeStamp3 }));
    returnedEntries.push(
        self.performance.measure("measure12", { end: undefined }));
    returnedEntries.push(
        self.performance.measure("measure13", { end: 'mark1' }));
    returnedEntries.push(
        self.performance.measure("measure14", { start: timeStamp3, end: 'mark1' }));
    returnedEntries.push(
        self.performance.measure("measure15", { start: timeStamp1, end: timeStamp2, detail: undefined }));
    returnedEntries.push(
        self.performance.measure("measure16", { start: 'mark1', end: undefined, detail: null }));
    returnedEntries.push(
        self.performance.measure("measure17", { start: timeStamp3, end: 'mark2', detail: { customInfo: 159 }}));
    returnedEntries.push(
        self.performance.measure("measure18", { start: timeStamp1, duration: timeStamp2 - timeStamp1 }));
    returnedEntries.push(
        self.performance.measure("measure19", { duration: timeStamp2 - timeStamp1, end: timeStamp2 }));
    // {}, null, undefined, invalid-dict passed to startOrOptions are interpreted as start time being 0.
    returnedEntries.push(self.performance.measure("measure20", {}, 'mark1'));
    returnedEntries.push(self.performance.measure("measure21", null, 'mark1'));
    returnedEntries.push(self.performance.measure("measure22", undefined, 'mark1'));
    returnedEntries.push(self.performance.measure("measure23", { invalidDict:1 }, 'mark1'));
    checkEntries(returnedEntries, expectedEntries);
}, "measure entries' detail and start/end are customizable");

test(function() {
    this.add_cleanup(cleanupPerformanceTimeline);
    assert_throws_js(TypeError, function() {
      self.performance.measure("optionsAndNumberEnd", {'start': 2}, 12);
    }, "measure should throw a TypeError when passed an options object and an end time");
    assert_throws_js(TypeError, function() {
      self.performance.measure("optionsAndMarkEnd", {'start': 2}, 'mark1');
    }, "measure should throw a TypeError when passed an options object and an end mark");
    assert_throws_js(TypeError, function() {
      self.performance.measure("negativeStartInOptions", {'start': -1});
    }, "measure cannot have a negative time stamp.");
    assert_throws_js(TypeError, function() {
      self.performance.measure("negativeEndInOptions", {'end': -1});
    }, "measure cannot have a negative time stamp for end.");
}, "measure should throw a TypeError when passed an invalid argument combination");

