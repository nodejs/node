// unquoted attributes should be ok in non-strict mode
// https://github.com/isaacs/sax-js/issues/31
require(__dirname).test
  ( { xml :
      "<span class=test hello=world></span>"
    , expect :
      [ [ "attribute", { name: "class", value: "test" } ]
      , [ "attribute", { name: "hello", value: "world" } ]
      , [ "opentag", { name: "SPAN",
                       attributes: { class: "test", hello: "world" } } ]
      , [ "closetag", "SPAN" ]
      ]
    , strict : false
    , opt : {}
    }
  )

