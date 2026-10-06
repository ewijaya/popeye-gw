'use strict';
var timers = {
  setTimeout: function(fn, ms) { return setTimeout(fn, ms); },
  clearTimeout: function(id) { clearTimeout(id); }
};
require('./app')({
  Clay: require('@rebble/clay'), pebble: Pebble,
  storage: typeof localStorage !== 'undefined' ? localStorage : null,
  geolocation: typeof navigator !== 'undefined' ? navigator.geolocation : null,
  now: function() { return Math.floor(Date.now() / 1000); }, timers: timers,
  get: require('./http')(function() { return new XMLHttpRequest(); }, timers),
  log: function(message) { console.log(message); }
});
