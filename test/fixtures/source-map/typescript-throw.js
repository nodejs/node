var ATrue;
(function (ATrue) {
    ATrue[ATrue["IsTrue"] = 1] = "IsTrue";
    ATrue[ATrue["IsFalse"] = 0] = "IsFalse";
})(ATrue || (ATrue = {}));
if (false) {
    console.info('unreachable');
}
else if (true) {
    console.info('reachable');
}
else {
    console.info('unreachable');
}
function branch(a) {
    if (a === ATrue.IsFalse) {
        console.info('a = false');
    }
    else if (a === ATrue.IsTrue) {
        throw Error('an exception');
    }
    else {
        console.info('a = ???');
    }
}
branch(ATrue.IsTrue);
//# sourceMappingURL=typescript-throw.js.map
