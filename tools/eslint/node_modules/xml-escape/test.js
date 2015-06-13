var test = require('tape');
var escape = require('./index');
test("Characters should be escaped properly", function (t) {
    t.plan(1);

    t.equals(escape('" \' < > &'), '&quot; &apos; &lt; &gt; &amp;');
})