//sample command line args
// push tests/config http://localhost:5984/node_couchapp_db
//sample AfterPushSync handler, illustrating the tasks that can be done in the handler
var listener = {
  onAfterPushSync: function () {
    console.log("executing onAfterPushSync");
    //difficult to pass parms to this function because callback mechanism was already written, so re-read process.argv to tell us configDirectory and couchURL
    var fs = require("fs")
      , path = require("path")
      , configDirectory = process.argv[4]
      , couchURL = process.argv[5]
      , staticDataFileNames = fs.readdirSync(path.join(__dirname, "/static_data"))
      , database = require("nano")(couchURL);
    //if there are staticDataFileNames then iterate through them and create/update the documents as appropriate
    if (staticDataFileNames) {
      staticDataFileNames.forEach(function (value, index) {
        var doc = fs.readFileSync(path.join(__dirname, "/static_data", value));
        var jSONDoc = JSON.parse(doc);
        //To avoid a conflict, if the document already exists then we'll get the _rev of the existing
        //document so it can be added to the JSON of the document to be written.
        database.get(jSONDoc._id, {revs_info: true}, function (err, body) {
          //ignore any error here: the first call is just to see if a _rev can be obtained
          if (!err) {
            if (body && body._rev) {
              jSONDoc._rev = body._rev;
            }
          }
          database.insert(jSONDoc, "", function (err, body, header) {
            if (err) {
              console.log('*** error on insert to CouchDB: ', err.message);
              return;
            }
            console.log('CouchDB insert successful')
            console.log(body);
          });
        });
      });
    }
  }
}

module.exports = listener;
