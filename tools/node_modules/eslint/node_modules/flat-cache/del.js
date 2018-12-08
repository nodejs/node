var rimraf = require( 'rimraf' ).sync;
var fs = require( 'graceful-fs' );

module.exports = function del( file ) {
  if ( fs.existsSync( file ) ) {
    //if rimraf doesn't throw then the file has been deleted or didn't exist
    rimraf( file, {
      glob: false
    } );
    return true;
  }
  return false;
};
