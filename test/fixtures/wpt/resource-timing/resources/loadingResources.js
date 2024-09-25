//Fetching the Stylesheet
var link = document.createElement("link");
link.rel = "stylesheet";
link.href = "../resources/empty_style.css";
document.head.appendChild(link);

// Fetching an image
var img = document.createElement("img");
img.src = "/images/blue.png";
img.alt = "Sample Image for testing initiator Attribute";
document.body.appendChild(img);

//Inserting a html document in an iframe
var iframe = document.createElement("iframe");
iframe.src = "../resources/green.html";
document.body.appendChild(iframe);

// Inserting a script element
var script = document.createElement("script");
script.src = "../resources/empty.js";
document.body.appendChild(script);
