"use strict";

function createRealm(t) {
    return new Promise(resolve => {
        const iframe = document.createElement("iframe");
        t.add_cleanup(() => { iframe.remove(); });
        iframe.onload = () => { resolve(iframe.contentWindow); };
        iframe.name = "dummy";
        iframe.src = "support/dummy-iframe.html";
        document.body.append(iframe);
    });
}
