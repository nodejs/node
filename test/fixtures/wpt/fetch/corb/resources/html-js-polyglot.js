<!--/*--><html><body><script type="text/javascript"><!--//*/

// This is a regression test for https://crbug.com/839425
// which found out that some script resources are served
// with text/html content-type and with a body that is
// both a valid html and a valid javascript.
window['html-js-polyglot.js'] = true;

//--></script></body></html>
