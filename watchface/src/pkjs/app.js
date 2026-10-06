'use strict';
var schema = require('./schema');
var Storage = require('./storage');
var Queue = require('./queue');
var Weather = require('./weather');
var World = require('./world').World;
var clayUrl = require('./clay-compat');
var SETTINGS_KEY = 'popeye-clock-settings-v1';

// Dependencies are explicit so the entire phone flow can be tested offline.
module.exports = function(options) {
  var storage = new Storage(options.storage), pebble = options.pebble;
  var saved = storage.read(SETTINGS_KEY);
  var settings = schema.sanitize(saved && saved.version === schema.version ? saved.settings : {}, schema.defaults);
  var acknowledged = null;
  var queue = new Queue(pebble, options.timers, options.log, function(tag, payload) {
    if (tag === 'settings') { acknowledged = schema.sanitize(payload, acknowledged || schema.defaults); }
  });
  function send(tag, payload) { queue.add(tag, payload); }
  var weather = new Weather({get: options.get, geolocation: options.geolocation,
    storage: storage, timers: options.timers, now: options.now, send: send, settings: settings});
  var world = new World({now: options.now, timers: options.timers, send: send, zone: settings.WorldZone});
  var presets = {};
  schema.presetIds.forEach(function(id) { presets[id] = schema.preset(id); });
  var clay = new options.Clay(require('./config'), require('./config-custom'),
    {autoHandleEvents: false, userData: {presets: presets, presentation: schema.presentation}});
  function persist() {
    storage.write(SETTINGS_KEY, {version: schema.version, settings: settings});
    try { clay.setSettings(settings); } catch (e) { options.log('Phone settings could not be stored; watch updates still work.'); }
  }
  function configure() {
    // Set sanitized settings before opening so raw/corrupt Clay storage cannot
    // overwrite the versioned preferences. Refresh meta status without locations.
    try { clay.setSettings(settings); } catch (e) { /* localStorage can be unavailable */ }
    if (clay.meta && clay.meta.userData) { clay.meta.userData.weatherStatus = weather.statusText(); }
    try { pebble.openURL(clayUrl(clay)); }
    catch (e) { options.log('Could not open phone settings.'); }
  }
  function save(event) {
    if (!event || typeof event.response !== 'string' || !event.response || event.response === 'CANCELLED') { return; }
    var patch;
    try { patch = clay.getSettings(event.response, false); }
    catch (e) {
      // Clay persists while parsing. If storage is unavailable, independently
      // parse the same response so a saved form can still reach the watch.
      try { patch = JSON.parse(/^\{/.test(event.response) ? event.response : decodeURIComponent(event.response)); }
      catch (ignored) { options.log('Ignored invalid configuration response.'); return; }
    }
    if (!patch || typeof patch !== 'object' || Array.isArray(patch)) { return; }
    var previous = settings;
    settings = schema.sanitize(patch, previous);
    persist();
    // Compare with acknowledged values, not simply the preceding save: a failed
    // older save must be included in later retries/saves until the watch gets it.
    var payload = schema.wire(settings, acknowledged, false);
    if (settings.Preset !== previous.Preset) { payload[schema.keys.Preset] = settings.Preset; payload[schema.keys.ConfigVersion] = 1; }
    // A no-change save still resends a compact full snapshot: this recovers a
    // previous failed delivery. Never reapply a preset unless it changed.
    if (!Object.keys(payload).length) { payload = schema.wire(settings, null, false); }
    send('settings', payload);
    weather.changed(settings);
    world.refresh(settings.WorldZone);
  }
  pebble.addEventListener('showConfiguration', configure);
  pebble.addEventListener('webviewclosed', save);
  pebble.addEventListener('ready', function() {
    persist();
    send('settings', schema.wire(settings, null, false));
    var payload = {}; payload[schema.keys.PhoneReady] = 1; send('ready', payload);
    world.refresh(settings.WorldZone); weather.request();
  });
  pebble.addEventListener('appmessage', function(event) {
    var payload = event && event.payload || {};
    var mask = +((typeof payload.RequestData !== 'undefined') ? payload.RequestData : payload[schema.keys.RequestData]);
    if (mask & 1) { weather.request(); }
    if (mask & 2) { world.refresh(settings.WorldZone); }
  });
  return {getSettings: function() { return schema.copy(settings); }, clay: clay,
    queue: queue, weather: weather, world: world};
};
