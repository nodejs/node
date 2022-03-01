'use strict';
    
// Check if we have Javascript support
document.querySelector(':root').classList.add('has-js');

// Restore user mode preferences
const kCustomPreference = 'customDarkTheme';
const userSettings = sessionStorage.getItem(kCustomPreference);
const themeToggleButton = document.getElementById('theme-toggle-btn');
if (userSettings === null && window.matchMedia) {
  const mq = window.matchMedia('(prefers-color-scheme: dark)');
  if ('onchange' in mq) {
    function mqChangeListener(e) {
      document.documentElement.classList.toggle('dark-mode', e.matches);
    }
    mq.addEventListener('change', mqChangeListener);
    if (themeToggleButton) {
      themeToggleButton.addEventListener('click', function() {
        mq.removeEventListener('change', mqChangeListener);
      }, { once: true });
    }
  }
  if (mq.matches) {
    document.documentElement.classList.add('dark-mode');
  }
} else if (userSettings === 'true') {
  document.documentElement.classList.add('dark-mode');
}
if (themeToggleButton) {
  themeToggleButton.hidden = false;
  themeToggleButton.addEventListener('click', function() {
    sessionStorage.setItem(
      kCustomPreference,
      document.documentElement.classList.toggle('dark-mode')
    );
  });
}

// Handle pickers with click/taps rather than hovers
const pickers = document.querySelectorAll('.picker-header');
for(const picker of pickers) {
  picker.addEventListener('click', e => {
    if (!e.target.closest('.picker')) {
      e.preventDefault();
    }

    if (picker.classList.contains('expanded')) {
      picker.classList.remove('expanded');
    } else {
      for (const other of pickers) {
        other.classList.remove('expanded');
      }

      picker.classList.add('expanded');
    }
  });
}

// Track when the header is in sticky position
const header = document.querySelector(".header");
let ignoreNextIntersection = false;
new IntersectionObserver(
  ([e]) => {
    const currentStatus = header.classList.contains('is-pinned');
    const newStatus = e.intersectionRatio < 1;

    // Same status, do nothing
    if(currentStatus === newStatus) {
      return;
    } else if(ignoreNextIntersection) { 
      ignoreNextIntersection = false;
      return;
    }

    /*
      To avoid flickering, ignore the next change event that is triggered
      as the visible elements in the header change once we pin it.

      The timer is reset anyway after few milliseconds
    */
    ignoreNextIntersection = true;
    setTimeout(() => ignoreNextIntersection = false, 50);

    header.classList.toggle('is-pinned', newStatus);
  },
  { threshold: [1] }
).observe(header);