$(function(){
  highlight(undefined, undefined, 'pre');
  
  $('<a href="#toc">Hide</a>').appendTo($("#toc h2")).toggle(function(e){
    $("#toc ul").hide();
    $(this).text("Show");
    e.preventDefault();
  }, function(e){
    $("#toc ul").show();
    $(this).text("Hide");
    e.preventDefault();
  });
});
