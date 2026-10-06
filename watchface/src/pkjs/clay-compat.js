'use strict';

// @rebble/clay 1.1.0 ships ES5 phone code, but its embedded WebView framework
// contains three kinds of `let` declaration in the val/radiogroup manipulators.
// Those variables are local to their functions and need no block scope. Convert
// only these known declarations inside script tags for older mobile WebViews.
// No package files are changed; both native data URLs and emulator proxy URLs
// retain Clay's original return route and complete offline content.
module.exports = function(clay) {
  var url = clay.generateUrl();
  var parts = /^(data:[^,]+,|https?:\/\/[^#]+#)([\s\S]+)$/.exec(url);
  if (!parts) { return url; }
  var html = decodeURIComponent(parts[2]);
  html = html.replace(/(<script\b[^>]*>)([\s\S]*?)(<\/script>)/gi, function(all, open, script, close) {
    return open + script.replace(/\blet (value|integerValue|currentValue) = /g, 'var $1 = ') + close;
  });
  return parts[1] + encodeURIComponent(html);
};
