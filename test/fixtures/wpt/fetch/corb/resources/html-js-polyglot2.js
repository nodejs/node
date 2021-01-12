<!-- comment --> <script type='text/javascript'>
//<![CDATA[

// This is a regression test for https://crbug.com/839945
// which found out that some script resources are served
// with text/html content-type and with a body that is
// both a valid html and a valid javascript.
window['html-js-polyglot2.js'] = true;

//]]>--></script>
