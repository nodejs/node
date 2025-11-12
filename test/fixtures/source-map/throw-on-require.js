var ATrue;
(function (ATrue) {
    ATrue[ATrue["IsTrue"] = 1] = "IsTrue";
    ATrue[ATrue["IsFalse"] = 0] = "IsFalse";
})(ATrue || (ATrue = {}));
if (false) {
    console.info('unreachable');
}
else if (true) {
    throw Error('throw early');
}
else {
    console.info('unreachable');
}
//# sourceMappingURL=throw-on-require.js.map