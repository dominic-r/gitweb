/* cgit.js: javascript functions for cgit
 *
 * Copyright (C) 2006-2018 cgit Development Team <cgit@lists.zx2c4.com>
 *
 * Licensed under GNU General Public License v2
 *   (see COPYING for full license text)
 */

/* Theme management */
var THEME_KEY = 'cgit-theme';
var THEMES = ['auto', 'light', 'dark'];

function getStoredTheme() {
	try {
		return localStorage.getItem(THEME_KEY);
	} catch (e) {
		return null;
	}
}

function getCurrentTheme() {
	return getStoredTheme() || 'auto';
}

function setTheme(theme) {
	try {
		if (theme === 'auto') {
			document.documentElement.removeAttribute('data-theme');
			localStorage.removeItem(THEME_KEY);
		} else {
			document.documentElement.setAttribute('data-theme', theme);
			localStorage.setItem(THEME_KEY, theme);
		}
	} catch (e) {
		if (theme !== 'auto') {
			document.documentElement.setAttribute('data-theme', theme);
		}
	}
	updateThemeToggle();
}

function cycleTheme() {
	var current = getCurrentTheme();
	var idx = THEMES.indexOf(current);
	var next = THEMES[(idx + 1) % THEMES.length];
	setTheme(next);
}

function updateThemeToggle() {
	var toggle = document.getElementById('theme-toggle');
	if (toggle) {
		toggle.textContent = getCurrentTheme();
	}
}

function initTheme() {
	var stored = getStoredTheme();
	if (stored) {
		document.documentElement.setAttribute('data-theme', stored);
	}
}

// Initialize theme immediately
initTheme();

(function () {

/* Age rendering - follows the logic and suffixes used in ui-shared.c */

var age_classes = [ "age-mins", "age-hours", "age-days",    "age-weeks",    "age-months",    "age-years" ];
var age_suffix =  [ "min.",     "hours",     "days",        "weeks",        "months",        "years",         "years" ];
var age_next =    [ 60,         3600,        24 * 3600,     7 * 24 * 3600,  30 * 24 * 3600,  365 * 24 * 3600, 365 * 24 * 3600 ];
var age_limit =   [ 7200,       24 * 7200,   7 * 24 * 7200, 30 * 24 * 7200, 365 * 25 * 7200, 365 * 25 * 7200 ];
var update_next = [ 10,         5 * 60,      1800,          24 * 3600,      24 * 3600,       24 * 3600,       24 * 3600 ];

function render_age(e, age) {
	var t, n;

	for (n = 0; n < age_classes.length; n++)
		if (age < age_limit[n])
			break;

	t = Math.round(age / age_next[n]) + " " + age_suffix[n];

	if (e.textContent != t) {
		e.textContent = t;
		if (n == age_classes.length)
			n--;
		if (e.className != age_classes[n])
			e.className = age_classes[n];
	}
}

function aging() {
	var n, next = 24 * 3600,
	    now_ut = Math.round((new Date().getTime() / 1000));

	for (n = 0; n < age_classes.length; n++) {
		var m, elems = document.getElementsByClassName(age_classes[n]);

		if (elems.length && update_next[n] < next)
			next = update_next[n];

		for (m = 0; m < elems.length; m++) {
			var age = now_ut - elems[m].getAttribute("data-ut");

			render_age(elems[m], age);
		}
	}

	window.setTimeout(aging, next * 1000);
}

/* Line highlighting for blob/tree view */

function initLineHighlight() {
	var table = document.querySelector('table.blob');
	if (!table)
		return;

	var linenoTd = table.querySelector('td.linenumbers');
	var linesTd = table.querySelector('td.lines');
	if (!linenoTd || !linesTd)
		return;

	/* Create overlay element for source-side highlight */
	var overlay = document.createElement('div');
	overlay.className = 'line-highlight-overlay';
	overlay.style.display = 'none';
	linesTd.style.position = 'relative';
	linesTd.insertBefore(overlay, linesTd.firstChild);

	/* Remove href so browser doesn't scroll on click */
	var anchors = linenoTd.querySelectorAll('a[id^="n"]');
	for (var i = 0; i < anchors.length; i++)
		anchors[i].removeAttribute('href');

	var lastClicked = null;

	function clearHighlight() {
		var old = linenoTd.querySelectorAll('.line-hl');
		for (var i = 0; i < old.length; i++)
			old[i].classList.remove('line-hl');
		overlay.style.display = 'none';
	}

	function highlightRange(from, to) {
		clearHighlight();
		var lo = Math.min(from, to);
		var hi = Math.max(from, to);

		/* Highlight line number anchors */
		for (var n = lo; n <= hi; n++) {
			var anchor = document.getElementById('n' + n);
			if (anchor)
				anchor.classList.add('line-hl');
		}

		/* Position overlay on source side using anchor positions */
		var firstAnchor = document.getElementById('n' + lo);
		var lastAnchor = document.getElementById('n' + hi);
		if (!firstAnchor || !lastAnchor)
			return;

		var preEl = linesTd.querySelector('pre');
		if (!preEl)
			return;

		var preRect = preEl.getBoundingClientRect();
		var firstRect = firstAnchor.getBoundingClientRect();
		var lastRect = lastAnchor.getBoundingClientRect();

		var top = firstRect.top - preRect.top;
		var height = lastRect.bottom - firstRect.top;

		overlay.style.top = top + 'px';
		overlay.style.height = height + 'px';
		overlay.style.display = 'block';
	}

	function updateHash(from, to) {
		var hash;
		if (from === to)
			hash = '#n' + from;
		else
			hash = '#n' + Math.min(from, to) + '-' + Math.max(from, to);
		history.replaceState(null, '', hash);
	}

	function parseHash() {
		var h = location.hash;
		if (!h)
			return null;
		/* #n42 or #n10-20 */
		var m = h.match(/^#n(\d+)(?:-(\d+))?$/);
		if (!m)
			return null;
		var from = parseInt(m[1], 10);
		var to = m[2] ? parseInt(m[2], 10) : from;
		return { from: from, to: to };
	}

	/* Prevent text selection in line numbers */
	linenoTd.addEventListener('selectstart', function(e) {
		e.preventDefault();
	});

	function getLineNum(el) {
		var a = el ? el.closest('a[id^="n"]') : null;
		if (!a) return null;
		var n = parseInt(a.id.substring(1), 10);
		return isNaN(n) ? null : n;
	}

	var dragStart = null;

	linenoTd.addEventListener('mousedown', function(e) {
		if (e.button !== 0)
			return;
		var num = getLineNum(e.target);
		if (num === null)
			return;
		e.preventDefault();
		var sel = window.getSelection();
		if (sel) sel.removeAllRanges();

		if (e.shiftKey && lastClicked !== null) {
			highlightRange(lastClicked, num);
			updateHash(lastClicked, num);
		} else {
			dragStart = num;
			lastClicked = num;
			highlightRange(num, num);
			updateHash(num, num);
		}
	});

	linenoTd.addEventListener('mouseover', function(e) {
		if (dragStart === null)
			return;
		var num = getLineNum(e.target);
		if (num === null)
			return;
		var sel = window.getSelection();
		if (sel) sel.removeAllRanges();
		highlightRange(dragStart, num);
	});

	document.addEventListener('mouseup', function(e) {
		if (dragStart === null)
			return;
		var num = getLineNum(e.target);
		if (num !== null)
			updateHash(dragStart, num);
		dragStart = null;
	});

	/* Highlight from URL hash on load */
	var initial = parseHash();
	if (initial) {
		highlightRange(initial.from, initial.to);
		lastClicked = initial.from;
		var target = document.getElementById('n' + initial.from);
		if (target)
			target.scrollIntoView({ block: 'center' });
	}

	/* React to back/forward navigation */
	window.addEventListener('hashchange', function() {
		var range = parseHash();
		if (range) {
			highlightRange(range.from, range.to);
			lastClicked = range.from;
		} else {
			clearHighlight();
			lastClicked = null;
		}
	});
}

document.addEventListener("DOMContentLoaded", function() {
	updateThemeToggle();
	aging();
	initLineHighlight();
}, false);

})();
