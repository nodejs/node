function takeScreenshot() {
    document.documentElement.classList.remove("reftest-wait");
}

function takeScreenshotDelayed(timeout) {
    setTimeout(function() {
        takeScreenshot();
    }, timeout);
}
