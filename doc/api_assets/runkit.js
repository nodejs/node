(function () {
    if (window.NodeList && !NodeList.prototype.forEach)
        NodeList.prototype.forEach = Array.prototype.forEach;

    var runnables = document.querySelectorAll(".runkit");

    if (runnables.length <= 0)
        return;

    var script = document.createElement("script");

    script.onload = function () {

        var version = document
            .querySelector('meta[name="nodejs.org:node-version"]')
            .content;

        RunKit
            .getMaximumAvailableNodeVersionInRange(version)
            .then(function (nodeVersion) {
                if (!nodeVersion)
                    return;

                runnables.forEach(function (runnable) {
                    var replace = runnable.parentNode;
                    RunKit.createNotebook({
                        element: replace,
                        source: RunKit.sourceFromElement(replace),
                        clearParentContents: true,
                        minHeight: '0px',
                        nodeVersion: nodeVersion,
                        evaluateOnLoad: RunKit.__nodeShouldEvaluateOnLoad(),
                        onLoad: function () {
                            replace.style.background = "none";
                            replace.style.overflow = "visible";
                            replace.style.border = "none";
                        }
                    });
                });
            });
    };

    script.src = "https://embed.runkit.com";

    document.body.append(script);
})();
