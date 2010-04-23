var NodeDoc = {};

NodeDoc.init = function()
{
	NodeDoc.generateToc();
	NodeDoc.setupScrollUpdate();
	NodeDoc.setupSmoothScrolling();

	// Unblock rendering
	setTimeout(function()
	{
		sh_highlightDocument();
	}, 100);
};
NodeDoc.generateToc = function()
{
	var cur_level, last_level = 0, html = [];

	$('h2, h3').each(function(i)
	{
		var $this = $(this);
		$this.attr('id', $this.text().replace(/\(.*\)$/gi, '').replace(/[\s\.]+/gi, '-').replace(/('|"|:)/gi, '').toLowerCase() + '-' + i);

		cur_level = this.tagName.substr(1, 1);

		if (cur_level > last_level) 
		{
			html.push('<ul><li>');
		}
		else if (cur_level < last_level)
		{
			html.push('</ul>');
		}

		if (cur_level == last_level || cur_level < last_level)
		{
			html.push('<li>');
		}

		html.push('<a href="#' + $this.attr('id') + '">' + $this.text().replace(/\(.*\)$/gi, '') + '</a>');
		if (cur_level == last_level || cur_level > last_level)
		{
			html.push('</li>');
		}

		last_level = cur_level;
	});

	html.push('</ul>');

	var $toc = $('#toc').append(html.join('')).find('ul li ul').each(function()
	{
		$(this).parent().prepend('<a href="#" class="toggler">+</a>');
	}).hide();

	$('.toggler').live('click', function()
	{
		var $toggler = $('ul', $(this).parent());

		if (!$toggler.is(':visible'))
		{
			$toggler.slideDown();
			$(this).text('–');
		}
		else
		{
			$toggler.slideUp();
			$(this).text('+');
		}

		return false;
	});
	
	$('#toc > ul > li').live('click', function(e)
	{
		if ($(e.target).parents('ul').length < 2)
		{
			$(this).closest('li').find('.toggler').click();
		}
	});
};
NodeDoc.setupScrollUpdate = function()
{
	$.extend($.expr[':'],{
		text: function(a,c, arr)
		{
			return $.trim($(a).text()) === (arr[3] || 'av34');
		}
	});
	var $headlines = $('h2');
	var scrollTimeout;

	function updateNavigation()
	{
		var bodyCenter = $('body').scrollTop()+10;

		var $last = $('<div id="dummy"/>');
		var found = false;

		$headlines.each(function(index)
		{
			var $this = $(this);

			if ($this.offset().top > bodyCenter)
			{
				if (scrollTimeout)
				{
					clearTimeout(scrollTimeout);
				}

				scrollTimeout = setTimeout(function()
				{
					updateNav($last);
				}, 100);
				return false;
			}

			$last = $this;
		});

		if ($last.is('#dummy'))
		{
			$('.current-section').remove();
		}

		function updateNav($last)
		{
			var $activeToc = $('#toc > ul > li > a:text("'+$last.text()+'")').parent().addClass('active').siblings().removeClass('active').end();
			
			if ($activeToc.length)
			{
				var newHash = $activeToc.find('> a:not(.toggler)').attr('href');

				var $elementHash = $('#man h2'+newHash);

				$('title').text($('title').text().replace(/-- (.*) for/, '-- '+$last.text()+' for'));

				$('.current-section').remove();
				$currentSelection = $last.clone().width($last.width()).addClass('current-section');
				$last.after($currentSelection);
			}
			
		}
	}

	updateNavigation();

	$('#man').scroll(updateNavigation);
};
NodeDoc.setupSmoothScrolling = function()
{
	$('a[href*="#"]').live('click', function()
	{
		var $this = $(this);
		if (location.pathname.replace(/^\//, '') == this.pathname.replace(/^\//, '') && location.hostname == this.hostname)
		{
			var $target = $(this.hash);
			$target = $target.length && $target || $('[name=' + this.hash.slice(1) + ']');

			if ($target.length)
			{
				var targetOffset = $('#man').scrollTop()+$target.offset().top;
				
				if ($this.closest('#toc').length && $this.parents('ul').length > 1)
				{
					targetOffset -= 45;
				}

				$('#man').animate({
					scrollTop: targetOffset
				}, 200);

				return false;
			}
		}
	});
};
NodeDoc.init();