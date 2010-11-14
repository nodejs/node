/*
  Process a single doc file

  argv[2] = template file
  argv[3] = input file
  argv[4] = output file

*/
var fs = require("fs"),
    path = require("path"),
    markdown = require("./markdown"),
    argv = process.argv,
    argc = argv.length;

var template = fs.readFileSync(argv[2], "utf8");

var ids = {};

function formatIdString(str){
  str = str
    .replace(/\([^)}]*\)/gmi, "")
    .replace(/[^A-Za-z0-9_.]+/gmi, "_");

  return str.substr(0,1).toLowerCase() + str.substr(1);
}


var includeExpr = /^@include\s+([A-Za-z0-9-_]+)(?:\.)?([a-zA-Z]*)$/gmi;
function convertData(data, current_file){
  // Allow including other pages in the data.
  function loadIncludes(data){
    return data.replace(includeExpr, function(src, name, ext){
      try {
        var include_path = path.join(current_file, "../", name+"."+(ext || "markdown"))
        return loadIncludes(fs.readFileSync(include_path, "utf8"));
      } catch(e) {
        return "";
      }
    });
  };

  data = loadIncludes(data);

  // Convert it to HTML from Markdown
  if(data.length == 0){
    data = "Sorry, this section is currently undocumented, but we'll be working on it.";
  }

  data = markdown.toHTML(markdown.parse(data), {xhtml:true});

  data = data.replace(/<hr><\/hr>/g, "<hr />");

  data = data.replace(/(\<h[2-6])\>([^<]+)(\<\/h[1-6]\>)/gmi, function(o, ts, c, te){
    var id = formatIdString(c);
    return ts+' id="'+id+'">'+c+te;
  });

  return data;
};

if(argc > 3){
  var filename = argv[3];

  fs.readFile(filename, "utf8", function(err, data){
    if(err) throw err;

    // do conversion stuff.
    var html = convertData(data, filename);
    var output = template.replace("{{content}}", html);

    filename = path.basename(filename, '.markdown');

    if(filename == "index"){
      output = output.replace("{{section}}", "");
      output = output.replace(/<body([^>]*)>/, '<body class="index" $1>');
    } else {
      output = output.replace("{{section}}", filename+" - ")
    }

    if(argc > 4) {
      fs.writeFile(argv[4], output);
    } else {
      process.stdout.write(output);
    }
  });
}
