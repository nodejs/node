$(function() {
  var $toc = $('#toc');
  $('h2').each(function() {
    var
      $h2 = $(this),
      $div = $('<div class="toclevel1" />'),
      $a = $('<a/>')
        .text($h2.text())
        .attr('href', '#'+$h2.attr('id'));

    $toc.append($div.append($a));
  });

  sh_highlightDocument();
});