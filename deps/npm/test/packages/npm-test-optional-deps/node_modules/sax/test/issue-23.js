
require(__dirname).test
  ( { xml :
      "<compileClassesResponse>"+
        "<result>"+
          "<bodyCrc>653724009</bodyCrc>"+
          "<column>-1</column>"+
          "<id>01pG0000002KoSUIA0</id>"+
          "<line>-1</line>"+
          "<name>CalendarController</name>"+
          "<success>true</success>"+
        "</result>"+
      "</compileClassesResponse>"

    , expect :
      [ [ "opentag", { name: "COMPILECLASSESRESPONSE", attributes: {} } ]
      , [ "opentag", { name : "RESULT", attributes: {} } ]
      , [ "opentag", { name: "BODYCRC", attributes: {} } ]
      , [ "text", "653724009" ]
      , [ "closetag", "BODYCRC" ]
      , [ "opentag", { name: "COLUMN", attributes: {} } ]
      , [ "text", "-1" ]
      , [ "closetag", "COLUMN" ]
      , [ "opentag", { name: "ID", attributes: {} } ]
      , [ "text", "01pG0000002KoSUIA0" ]
      , [ "closetag", "ID" ]
      , [ "opentag", {name: "LINE", attributes: {} } ]
      , [ "text", "-1" ]
      , [ "closetag", "LINE" ]
      , [ "opentag", {name: "NAME", attributes: {} } ]
      , [ "text", "CalendarController" ]
      , [ "closetag", "NAME" ]
      , [ "opentag", {name: "SUCCESS", attributes: {} } ]
      , [ "text", "true" ]
      , [ "closetag", "SUCCESS" ]
      , [ "closetag", "RESULT" ]
      , [ "closetag", "COMPILECLASSESRESPONSE" ]
      ]
    , strict : false
    , opt : {}
    }
  )

