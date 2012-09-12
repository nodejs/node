// https://github.com/isaacs/sax-js/issues/47
require(__dirname).test
  ( { xml : '<a href="query.svc?x=1&y=2&z=3"/>'
    , expect : [ 
        [ "attribute", { name:'href', value:"query.svc?x=1&y=2&z=3"} ],
        [ "opentag", { name: "a", attributes: { href:"query.svc?x=1&y=2&z=3"} } ],
        [ "closetag", "a" ]
      ]
    , strict : true
    , opt : {}
    }
  )

