
function add_toc_tree_to_leftnav(force){
    if (force) document.querySelector('#toc_in_leftnav')?.remove();

    var toc_in_leftnav = document.querySelector('#toc_in_leftnav');
    var toc_tree, toc_tree_ul, left_nav_tree;
    if (!toc_in_leftnav) {
        console.log('href', document.location.href, 'nodejs doc add toc_tree');

        const leftnav = document.querySelector('#column2.interior');
        if (leftnav) {
            leftnav.style.overflow = 'scroll'; // default x is hidden
        }

        toc_tree = document.querySelector('#toc-picker > li')?.cloneNode(true);
        left_nav_tree = document.querySelector('#content > #column2');
        if (!toc_tree || !left_nav_tree) {
            console.warn('not found nodejs doc ?')
            return false;
        }

        toc_tree.id = 'toc_in_leftnav';
        left_nav_tree.insertAdjacentElement('afterbegin', toc_tree);

        toc_tree.insertAdjacentHTML('afterbegin', `
          <div style="position:sticky; top: 1px; margin-left: 30px; margin-bottom: 20px;">
            <button onclick="setDetailOpen(true)"  title="table of contents  Expend All"       > +    </button>
            <button onclick="setDetailOpen(false)" title="table of contents  Collapse All"     > -    </button>
            <button onclick="setLeftNavWidth(-50)" title="left navigation  Reduce width"       > &lt; </button>
            <button onclick="setLeftNavWidth(0)"   title="left navigation  Reset default width"> |    </button>
            <button onclick="setLeftNavWidth(50)"  title="left navigation  Extend width"       > &gt; </button>
          </div>`);
        toc_tree_ul = toc_tree.querySelector('ul');

        toc_tree_ul.querySelectorAll('li > ul').forEach(ul => {
            const li = ul.parentElement;
            const detail1 = document.createElement('details');
            const summ = document.createElement('summary');
            detail1.title = 'show details ' + (li.querySelector('a')?.textContent || '');
            summ.innerHTML = '';
            summ.style.color = 'lightyellow';
            detail1.open = false;
            detail1.appendChild(summ);
            detail1.appendChild(ul);
            li.appendChild(detail1);
        });

        // TODO use css?
        toc_tree_ul.querySelectorAll('*').forEach(x => {
            x.style.marginTop = '0px';
            x.style.marginBottom = '0px';
            x.style.paddingTop = '0px';
            x.style.paddingBottom = '0px';
            x.style.textWrapMode = 'nowrap';
            x.style.fontSize = 'small';
        });
    };
}

document.addEventListener('DOMContentLoaded', () => add_toc_tree_to_leftnav())


function setDetailOpen(stat) {
    document.getElementById('toc_in_leftnav').querySelectorAll('details').forEach(x => x.open=stat);
}

function setLeftNavWidth(inc_px) {
    const leftnav = document.querySelector('#column2.interior');
    const rightcont = document.querySelector('#column1.interior');
    let cur_width = parseInt(leftnav.style.width);
    if (isNaN(cur_width)) {
        cur_width = 234;       // in css, default 234px
    }

    let pxstr = '';
    if (typeof inc_px == 'number' && !isNaN(inc_px) && inc_px != 0) {
        cur_width += inc_px;
        pxstr = '' + cur_width + 'px';
    }

    leftnav.style.width = pxstr;
    rightcont.style.marginLeft = pxstr;
}

