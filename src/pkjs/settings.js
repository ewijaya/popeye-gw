'use strict';
var keys = ['Orientation', 'Buttons', 'Swap', 'Vibrate', 'Sound', 'Light', 'Ghosts', 'Theme', 'Demo', 'Online'];
var toggles = ['Swap', 'Vibrate', 'Ghosts', 'Demo', 'Online'];
var config = require('./config');
var custom = require('./config-custom');
var clayUrl = require('./clay-compat');

// The watch is authoritative. Never send Clay's defaults or phone cache on ready.
// Fetch before opening; save only fields changed from that form's initial snapshot.
module.exports = function(options) {
  var pebble = options.pebble, timers = options.timers;
  var clay = new options.Clay(config, custom, {autoHandleEvents: false});
  var noticeClay = new options.Clay([
    {type: 'heading', defaultValue: 'Popeye G&W'}, {type: 'text', defaultValue: ''}
  ], null, {autoHandleEvents: false});
  var baseline = null, pending = null, timer = null;
  var sequence = Math.floor(options.now() % 1000000000) + 1;

  function value(raw, index) {
    if (raw && typeof raw === 'object') { raw = raw.value; }
    if (raw === true) { raw = 1; }
    if (raw === false) { raw = 0; }
    if (typeof raw === 'string' && /^[0-3]$/.test(raw)) { raw = +raw; }
    return typeof raw === 'number' && raw % 1 === 0 && raw >= 0 && raw <= (index === 4 ? 3 : 1) ? raw : null;
  }
  function snapshot(bytes) {
    if (!Array.isArray(bytes) || bytes.length !== keys.length) { return null; }
    var result = {};
    for (var i = 0; i < keys.length; ++i) {
      var v = value(bytes[i], i);
      if (v === null) { return null; }
      result[keys[i]] = v;
    }
    return result;
  }
  function stop() {
    if (timer !== null) { timers.clearTimeout(timer); }
    timer = null; pending = null;
  }
  function notice(message) {
    baseline = null;
    noticeClay.config[1].defaultValue = message;
    pebble.openURL(clayUrl(noticeClay));
  }
  function open(settings) {
    var form = {};
    keys.forEach(function(key) { form[key] = toggles.indexOf(key) >= 0 ? !!settings[key] : String(settings[key]); });
    // Put actual values in config too: Clay's storage may be unavailable.
    clay.config[1].items.forEach(function(item) { item.defaultValue = form[item.messageKey]; });
    clay.meta.userData.values = form;
    try { clay.setSettings(form); } catch (e) { /* Defaults above still match the watch. */ }
    baseline = settings;
    pebble.openURL(clayUrl(clay));
  }
  function transmit() {
    if (!pending) { return; }
    var current = pending;
    ++current.attempts;
    timer = timers.setTimeout(function() {
      timer = null;
      if (pending !== current) { return; }
      if (current.attempts < 3) { transmit(); return; }
      stop();
      if (current.kind !== 'refresh') {
        notice(current.kind === 'save' ?
          'Could not confirm the save. Open Popeye G&W on your connected watch, then reopen Settings to check the values and try again.' :
          'Open Popeye G&W on your connected watch, then reopen Settings to load its current values.');
      }
    }, 2500);
    try { pebble.sendAppMessage(current.payload, function() {}, function() {
      // Delivery alone does not confirm persistence. Retry on the same bounded timer.
    }); } catch (e) { /* The same timer also handles a temporarily unavailable bridge. */ }
  }
  function request(kind, patch) {
    stop();
    var payload = {CFG_ID: ++sequence};
    if (patch) { payload.CFG_PATCH = patch; } else { payload.CFG_REQUEST = 1; }
    pending = {kind: kind, payload: payload, attempts: 0};
    transmit();
  }
  pebble.addEventListener('ready', function() { request('refresh'); });
  pebble.addEventListener('showConfiguration', function() { baseline = null; request('open'); });
  pebble.addEventListener('appmessage', function(event) {
    var payload = event && event.payload || {};
    var settings = snapshot(payload.CFG_DATA);
    if (!settings) { return; }
    if (!pending || payload.CFG_ID !== pending.payload.CFG_ID) { return; }
    var kind = pending.kind, status = payload.CFG_STATUS;
    stop();
    if (kind === 'open') { open(settings); }
    else if (kind === 'save' && status !== 0) {
      notice(status === 2 ? 'Finish or quit your round, then reopen Settings and save your changes.' :
        'The watch could not save these settings. Reopen Settings to load its saved values and try again.');
    }
  });
  pebble.addEventListener('webviewclosed', function(event) {
    if (!baseline) { return; }
    var initial = baseline;
    baseline = null;
    if (!event || !event.response || event.response === 'CANCELLED') { return; }
    var raw;
    // Parse without Clay's persistence side effect: an unacknowledged save is not authoritative.
    try { raw = JSON.parse(/^\{/.test(event.response) ? event.response : decodeURIComponent(event.response)); }
    catch (e) { return; }
    if (!raw || typeof raw !== 'object' || Array.isArray(raw)) { return; }
    var mask = 0, patch = [0, 0];
    for (var i = 0; i < keys.length; ++i) {
      var key = keys[i], v = raw[key] === undefined ? initial[key] : value(raw[key], i);
      if (v === null) { notice('Invalid settings. Reopen Settings and try again.'); return; }
      patch.push(v);
      if (v !== initial[key]) { mask |= 1 << i; }
    }
    if (!mask) { return; }
    patch[0] = mask & 255; patch[1] = mask >> 8;
    request('save', patch);
  });
  return {clay: function() { return clay; }};
};
