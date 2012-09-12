
require(__dirname).test({
  xml :
  "<root>"+
    "<child>" +
      "<haha />" +
    "</child>" +
    "<monkey>" +
      "=(|)" +
    "</monkey>" +
  "</root>",
  expect : [
    ["opentag", {
     "name": "root",
     "attributes": {}
    }],
    ["opentag", {
     "name": "child",
     "attributes": {}
    }],
    ["opentag", {
     "name": "haha",
     "attributes": {}
    }],
    ["closetag", "haha"],
    ["closetag", "child"],
    ["opentag", {
     "name": "monkey",
     "attributes": {}
    }],
    ["text", "=(|)"],
    ["closetag", "monkey"],
    ["closetag", "root"],
    ["end"],
    ["ready"]
  ],
  strict : true,
  opt : {}
});

