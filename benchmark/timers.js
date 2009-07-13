function next (i) {
  if (i <= 0) return;
  setTimeout(function () { next(i-1); }, 1);
}
next(700);
