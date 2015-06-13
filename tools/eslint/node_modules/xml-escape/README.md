xml-escape
==========

Escape XML in javascript (NodeJS)


npm install xml-escape

// Warning escape is a reserved word, so maybe best to use xmlescape for var name
var xmlescape = require('xml-escape');

xmlescape('"hello" \'world\' & false < true > -1')

// output
// '&quot;hello&quot; &apos;world&apos; &amp; true &lt; false &gt; -1'