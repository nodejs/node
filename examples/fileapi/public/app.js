function onFilesSelected(e) {
  var button = e.srcElement;
  button.disabled = true;
  var progress = document.querySelector('div#progress');
  progress.innerHTML = '0%';
  var files = e.target.files;
  var totalFiles = files.length;
  var filesSent = 0;
  if (totalFiles) {
    var uploader = new Uploader('ws://localhost:8080', function () {
      Array.prototype.slice.call(files, 0).forEach(function(file) {
        if (file.name == '.') {
          --totalFiles;
          return;
        }
        uploader.sendFile(file, function(error) {
          if (error) {
            console.log(error);
            return;
          }
          ++filesSent;
          progress.innerHTML = ~~(filesSent / totalFiles * 100) + '%';
          console.log('Sent: ' + file.name);
        });
      });
    });
  }
  uploader.ondone = function() {
    uploader.close();
    progress.innerHTML = '100% done, ' + totalFiles + ' files sent.';
  }
}

window.onload = function() {
  var importButtons = document.querySelectorAll('[type="file"]');
  Array.prototype.slice.call(importButtons, 0).forEach(function(importButton) {
    importButton.addEventListener('change', onFilesSelected, false);
  });
}
