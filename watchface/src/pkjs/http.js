'use strict';
// HTTPS only; XMLHttpRequest is available in old PebbleKit JS runtimes.
module.exports = function(xhrFactory, timers) {
  return function(url, done) {
    var xhr, timeout, ended = false;
    function finish(error, value) {
      if (ended) { return; }
      ended = true;
      if (timeout) { timers.clearTimeout(timeout); }
      done(error, value);
    }
    try {
      if (!/^https:\/\/(api|geocoding-api)\.open-meteo\.com\//.test(url)) {
        throw new Error('Unsupported weather endpoint');
      }
      xhr = xhrFactory();
      xhr.open('GET', url, true);
      xhr.onload = function() {
        if (xhr.status < 200 || xhr.status >= 300) { finish('http'); return; }
        try {
          var data = JSON.parse(xhr.responseText);
          if (!data || typeof data !== 'object' || data.error) { finish('response'); return; }
          finish(null, data);
        } catch (e) { finish('json'); }
      };
      xhr.onerror = function() { finish('network'); };
      xhr.ontimeout = function() { finish('timeout'); };
      timeout = timers.setTimeout(function() {
        finish('timeout');
        try { xhr.abort(); } catch (e) { /* already ended */ }
      }, 15000);
      xhr.send();
    } catch (e) { finish('network'); }
    return function() {
      ended = true;
      if (timeout) { timers.clearTimeout(timeout); }
      try { if (xhr) { xhr.abort(); } } catch (e) { /* cancellation is best effort */ }
    };
  };
};
