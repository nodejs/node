'use strict';
var readdirp =  require('..')
  , util     =  require('util')
  , fs       =  require('fs')
  , path     =  require('path')
  , es       =  require('event-stream')
  ;

function findLinesMatching (searchTerm) {

  return es.through(function (entry) {
    var lineno = 0
      , matchingLines = []
      , fileStream = this;

    function filter () {
      return es.mapSync(function (line) {
        lineno++;
        return ~line.indexOf(searchTerm) ? lineno + ': ' + line : undefined;
      });
    }

    function aggregate () {
      return es.through(
          function write (data) { 
            matchingLines.push(data); 
          }
        , function end () {

            // drop files that had no matches
            if (matchingLines.length) {
              var result = { file: entry, lines: matchingLines };

              // pass result on to file stream
              fileStream.emit('data', result);
            }
            this.emit('end');
          }
      );
    }

    fs.createReadStream(entry.fullPath, { encoding: 'utf-8' })

      // handle file contents line by line
      .pipe(es.split('\n'))

      // keep only the lines that matched the term
      .pipe(filter())

      // aggregate all matching lines and delegate control back to the file stream
      .pipe(aggregate())
      ;
  });
}

console.log('grepping for "arguments"');

// create a stream of all javascript files found in this and all sub directories
readdirp({ root: path.join(__dirname), fileFilter: '*.js' })

  // find all lines matching the term for each file (if none found, that file is ignored)
  .pipe(findLinesMatching('arguments'))

  // format the results and output
  .pipe(
    es.mapSync(function (res) {
      return '\n\n' + res.file.path + '\n\t' + res.lines.join('\n\t');
    })
  )
  .pipe(process.stdout)
  ;
