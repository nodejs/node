// https://github.com/isaacs/sax-js/issues/33
require(__dirname).test
  ( { xml : "<xml>\n"+
            "<!-- \n"+
            "  comment with a single dash- in it\n"+
            "-->\n"+
            "<data/>\n"+
            "</xml>"

    , expect :
      [ [ "opentag", { name: "xml", attributes: {} } ]
      , [ "text", "\n" ]
      , [ "comment", " \n  comment with a single dash- in it\n" ]
      , [ "text", "\n" ]
      , [ "opentag", { name: "data", attributes: {} } ]
      , [ "closetag", "data" ]
      , [ "text", "\n" ]
      , [ "closetag", "xml" ]
      ]
    , strict : true
    , opt : {}
    }
  )

