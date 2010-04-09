$(function() {
  var count = 0;
  var cur_level, last_level = 0, html = "";
  $(":header").filter("h2, h3").each(function(i, el){
    $(this).attr("id", $(this).text().replace(/\(.*\)$/gi, "").replace(/[\s\.]+/gi, "-").toLowerCase()+"-"+(count++))
    
    cur_level = el.tagName.substr(1,1);
    
    if(cur_level > last_level){
      html += "<ul>";
    } else if (cur_level < last_level){
      html += "</ul>"
    }
    if(i > 0){
      html += "</li>";
    }
    html += '<li>';
    html += '<a href="#'+$(el).attr("id")+'">'+$(el).text().replace(/\(.*\)$/gi, "")+"</a>";

    last_level = cur_level;
  });
  
  html += "</ul>";
  
  $("#toc").append(html);
  
  $("#toc ul li").addClass("topLevel");
  
  $("#toc ul li ul").each(function(i, el){
    $(el).parent().removeClass("topLevel").prepend('<a href="#" class="toggler">+</a>');
  })
  
  $("#toc ul li ul").hide();
  $("#toc ul li .toggler").bind("click", function(e){
    var el = $("ul", $(this).parent());
    if(el.css("display") == "none"){
      el.slideDown();
      $(this).text("–");
    } else {
      el.slideUp();
      $(this).text("+");
    }
    e.preventDefault();
    return false;
  });
  
  $('a[href*=#]').click(function() {
      if (location.pathname.replace(/^\//,'') == this.pathname.replace(/^\//,'')
      && location.hostname == this.hostname) {
        var $target = $(this.hash);
        $target = $target.length && $target
        || $('[name=' + this.hash.slice(1) +']');
        if ($target.length) {
          var targetOffset = $target.offset().top;
          $('html,body')
          .animate({scrollTop: targetOffset}, 500);
         return false;
        }
      }
    });
    
  sh_highlightDocument();
});