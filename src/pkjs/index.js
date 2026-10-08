'use strict';
require('../js/pebble-js-app');
require('./settings')({
  Clay: require('@rebble/clay'), pebble: Pebble,
  timers: {setTimeout: function(fn, ms) { return setTimeout(fn, ms); },
    clearTimeout: function(id) { clearTimeout(id); }},
  now: function() { return Date.now(); }
});
