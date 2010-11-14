$(function(){
  highlight(undefined, undefined, 'pre');
  var $headings = $("h2, h3, h4, h5, h6");

  if(! $("body").hasClass("index") && $headings.size() > 2){
    var current_level
      , last_level = 0
      , toc = [
        '<div id="toc">',
        '<h2>Table Of Contents <a id="toggler" href="#toc">Hide</a></h2>'
      ];

    for(var i=0, hl=$headings.size()+1; i < hl; i++) {
      var heading = $headings[i] || false;
      if(heading) {
        current_level = heading.tagName.substr(1,1);

        console.log(current_level, last_level, $(heading).text());

        if(last_level != 0 && current_level <= last_level) {
          toc.push("</li>");
        }

        if(current_level > last_level) {
          toc.push("<ul>");
          toc.push("<li>");
        } else if(current_level < last_level) {
          console.log(last_level-current_level);
          for(var c=last_level-current_level; 0 < c ; c-- ){
            toc.push("</ul>");
            toc.push("</li>");
          }
        }

        if(current_level == last_level || current_level < last_level) {
          toc.push("<li>");
        }

        toc.push('<a href="#'+$(heading).attr("id")+'">'+$(heading).text()+'</a>');
        last_level = current_level;
      } else {
        toc.push("</li>");
        toc.push("</ul>");
      }
    }

    toc.push("</li>");
    toc.push("</ul>");
    toc.push("<hr />");
    toc.push("</div>");

    $("#container header").after(toc.join("\n"));
    $("#toggler").toggle(function(e){
      $("#toc ul").hide();
      $(this).text("show");
      e.preventDefault();
    }, function(e){
      $("#toc ul").show();
      $(this).text("hide");
      e.preventDefault();
    });
  }
});
