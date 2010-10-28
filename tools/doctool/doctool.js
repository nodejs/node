var fs = require("fs")
  , path = require("path")
  , cp = require('child_process')
  , markdown = require("./markdown");

var cwd = process.cwd()
  , doc_root = path.join(cwd, "doc/api/")
  , build_root = path.join(cwd, "build/doc/api/")
  , assets_path = path.join(cwd, "doc/api_assets/")
  , bassets_path = path.join(build_root, "assets/");

/*
A simple step / flow-control pattern, so that I can make the code in this file
just a little bit more easy to follow.
*/
var step = function(){
  var self = this;
  this.steps = Array.prototype.slice.call(arguments);
  this.index = 0;
  this.next = function(){
    var index = self.index++;
    return function(){
      if(index < self.steps.length){
        self.steps[index](self.next());
      } else {
        return function(){};
      }
    };
  };
  return this.next();
};


var includeExpr = /^@include\s+([A-Za-z0-9-_]+).([a-zA-Z]+)$/gmi;
function convertData(data){
  // Allow including other pages in the data.
  data = data.replace(includeExpr, function(src, name, ext){
    try {
      return fs.readFileSync(path.join(doc_root, name+"."+ext), "utf8");
    } catch(e) {
      return "";
    }
  });
  
  // Convert it to HTML from Markdown
  if(data.length == 0){
    data = "Sorry, this section is currently undocumented, but we'll be working on it.";
  }
  
  return markdown.toHTML(markdown.parse(data), {xhtml:true});
};

/*
Ensures that the output directory exists, this can probably be done in the
makefile.
*/
function checkdir(next){
  fs.stat(build_root, function(err){
    if(err) {
      // easiest way to recursively create directories without doing loops.
      cp.exec("mkdir -p "+build_root, function(err, stdout, stderr){
        next();
      });
    } else {
      next();
    }
  })
};

/*
Loads the template for which the documentation should be outputed into.
*/
var template_index, template_page;

function loadTemplates(next){
  var templates_path = path.join(doc_root, "..", "template");
  
  fs.readFile(templates_path+"_page.html", "utf8", function(err, data){
    if(err) throw err;
    
    template_page = data;
    
    fs.readFile(templates_path+"_index.html", "utf8", function(err, data){
      if(err) throw err;

      template_index = data;
      next();
    });
  });
};


/*
This function reads the doc/api/* directory, and filters out any files 
that are not markdown files. It then converts the markdown to HTML, and 
outputs it into the previously loaded template file.
*/
function convertFiles(next){
  fs.readdir(doc_root, function(err, files){
    if(err) throw err;
    
    files.filter(function(file){
      return path.extname(file) == ".markdown";
    }).forEach(function(file){
      var filename = path.basename(file, '.markdown')
        , build_path = path.join(build_root, filename+".html")
        , doc_path = path.join(doc_root, file);

      fs.readFile(doc_path, "utf8", function(err, data){
        if(err) throw err;
        
        // do conversion stuff.
        var html = convertData(data);
        var output = template_page
          .replace("{{section}}", filename)
          .replace("{{content}}", html);
        
        fs.writeFile(build_path, output, function(err){
          if(err) throw err;
        });
      });
    });
  });
  // we don't need the next call to wait at all, so stick it here.
  next();
};

function createIndex(next){
  fs.readFile(path.join(doc_root, "_index.markdown"), "utf8", function(err, data){
    if(err) throw err;
    
    // do conversion stuff.
    var html = convertData(data);
    var output = template_index.replace("{{content}}", html);
    
    fs.writeFile(path.join(build_root, "index.html"), output, function(err){
      if(err) throw err;
    });
  });
  // one again, no need to wait.
  next();
};

function copyAssets(next){
  cp.exec("cp -R "+assets_path+" "+bassets_path, function(err, stdout, stderr){
    next();
  });
};

step(
  checkdir,
  copyAssets,
  loadTemplates,
  convertFiles,
  createIndex
)();