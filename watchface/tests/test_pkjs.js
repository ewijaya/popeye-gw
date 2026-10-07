'use strict';
// Entire suite is offline. No test uses the real phone, browser, GPS or network.
var assert = require('assert');
var fs = require('fs');
var path = require('path');
var vm = require('vm');
var schema = require('../src/pkjs/schema');
var config = require('../src/pkjs/config');
var custom = require('../src/pkjs/config-custom');
var Storage = require('../src/pkjs/storage');
var Queue = require('../src/pkjs/queue');
var Weather = require('../src/pkjs/weather');
var world = require('../src/pkjs/world');
var Http = require('../src/pkjs/http');
var createApp = require('../src/pkjs/app');
var K = schema.keys;
var tests = 0;
function test(name, run) { run(); tests++; console.log('ok ' + tests + ' - ' + name); }
function plain(value) { return JSON.parse(JSON.stringify(value)); }
function Backend() { this.values = {}; }
Backend.prototype.getItem = function(key) { return this.values[key] || null; };
Backend.prototype.setItem = function(key, value) { this.values[key] = String(value); };
function Clock() { this.ms = Date.UTC(2026, 9, 6, 12); this.ids = 0; this.jobs = {}; }
Clock.prototype.now = function() { return Math.floor(this.ms / 1000); };
Clock.prototype.setTimeout = function(fn, ms) { var id = ++this.ids; this.jobs[id] = {fn: fn, at: this.ms + ms}; return id; };
Clock.prototype.clearTimeout = function(id) { delete this.jobs[id]; };
Clock.prototype.advance = function(seconds) {
  var end = this.ms + seconds * 1000, runs = 0;
  while (true) {
    var chosen = null;
    Object.keys(this.jobs).forEach(function(id) {
      if (this.jobs[id].at <= end && (!chosen || this.jobs[id].at < this.jobs[chosen].at)) { chosen = id; }
    }, this);
    if (!chosen) { break; }
    if (++runs > 1000) { throw new Error('Runaway timer'); }
    var job = this.jobs[chosen]; delete this.jobs[chosen]; this.ms = job.at; job.fn();
  }
  this.ms = end;
};
function timers(clock) {
  return {setTimeout: function(fn, ms) { return clock.setTimeout(fn, ms); },
    clearTimeout: function(id) { clock.clearTimeout(id); }};
}
function Pebble() { this.listeners = {}; this.sent = []; this.urls = []; this.autoAck = true; }
Pebble.prototype.addEventListener = function(name, fn) { (this.listeners[name] = this.listeners[name] || []).push(fn); };
Pebble.prototype.emit = function(name, value) { (this.listeners[name] || []).forEach(function(fn) { fn(value); }); };
Pebble.prototype.sendAppMessage = function(payload, ok, bad) {
  this.sent.push({payload: plain(payload), ok: ok, bad: bad});
  if (this.autoAck) { ok(); }
};
Pebble.prototype.openURL = function(url) { this.urls.push(url); };
Pebble.prototype.getActiveWatchInfo = function() { return {platform: 'emery', firmware: {major: 4}}; };
Pebble.prototype.getAccountToken = function() { return 'test-account'; };
Pebble.prototype.getWatchToken = function() { return 'test-watch'; };
function Network() { this.calls = []; }
Network.prototype.get = function(url, done) {
  var call = {url: url, done: done, cancelled: false}; this.calls.push(call);
  return function() { call.cancelled = true; };
};
function data(clock, temp) {
  return {current: {time: clock.now() - 120, temperature_2m: temp, weather_code: 61},
    daily: {temperature_2m_max: [temp + 4], temperature_2m_min: [temp - 5]}};
}
function weatherHarness(settings, backend) {
  var clock = new Clock(), network = new Network(), sent = [], gps = [];
  var service = new Weather({get: network.get.bind(network), now: clock.now.bind(clock),
    timers: timers(clock), storage: new Storage(backend || new Backend()),
    geolocation: {getCurrentPosition: function(ok, bad, options) { gps.push({ok: ok, bad: bad, options: options}); }},
    settings: schema.sanitize(settings), send: function(tag, payload) { sent.push(payload); }});
  return {clock: clock, network: network, sent: sent, gps: gps, service: service};
}
function realClay(pebble, backend) {
  // Use the actual ES5 package artifact that the SDK aliases @rebble/clay to,
  // without npm's unbundled development index.js or any mock Clay parser.
  var sandbox = {module: {exports: {}}, exports: {}, Pebble: pebble,
    localStorage: backend, console: {log: function() {}, error: function() {}},
    require: function(name) { if (name === 'message_keys') { return K; } throw new Error('Unexpected dependency ' + name); }};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../node_modules/@rebble/clay/src/js/index.js'), 'utf8'), sandbox);
  return sandbox.module.exports;
}
function appHarness(backend) {
  var clock = new Clock(), pebble = new Pebble(), network = new Network(); backend = backend || new Backend();
  var app = createApp({Clay: realClay(pebble, backend), pebble: pebble, storage: backend,
    now: clock.now.bind(clock), timers: timers(clock), get: network.get.bind(network),
    geolocation: null, log: function() {}});
  return {app: app, pebble: pebble, network: network, clock: clock, backend: backend};
}
function Item(configItem) { this.config = configItem; this.value = configItem.defaultValue; this.listeners = {}; this.disabled = false; }
Item.prototype.get = function() { return this.config.serializeValueAs === 'integer' ? Number(this.value) : this.value; };
Item.prototype.set = function(value) {
  if (String(this.value) === String(value)) { return; }
  this.value = value; this.trigger('change');
};
Item.prototype.on = function(event, fn) { (this.listeners[event] = this.listeners[event] || []).push(fn); };
Item.prototype.trigger = function(event) { (this.listeners[event] || []).forEach(function(fn) { fn(); }); };
Item.prototype.enable = function() { this.disabled = false; };
Item.prototype.show = function() { this.hidden = false; };
Item.prototype.hide = function() { this.hidden = true; };
Item.prototype.disable = function() { this.disabled = true; };
function formHarness(saved) {
  var items = {}, ids = {}, built;
  function visit(list) {
    list.forEach(function(c) {
      if (c.items) { visit(c.items); } else {
        var item = new Item(c);
        if (c.messageKey) { items[c.messageKey] = item; if (saved && saved[c.messageKey] !== undefined) { item.value = saved[c.messageKey]; } }
        if (c.id) { ids[c.id] = item; }
      }
    });
  }
  visit(config);
  var presets = {}; schema.presetIds.forEach(function(id) { presets[id] = schema.preset(id); });
  custom.call({EVENTS: {AFTER_BUILD: 'built'}, on: function(event, fn) { built = fn; },
    meta: {userData: {presets: presets, presentation: schema.presentation, groups: schema.groups(config), pairable: schema.pairable}},
    getItemByMessageKey: function(name) { return items[name]; }, getItemById: function(name) { return ids[name]; }});
  built(); return {items: items, ids: ids};
}

test('wire map matches package exactly and every active setting has one offline form control', function() {
  var pkg = require('../package.json'), seen = {};
  assert.deepStrictEqual(pkg.pebble.messageKeys, K);
  function visit(items) { items.forEach(function(item) { if (item.items) { visit(item.items); }
    else if (item.messageKey) { assert(!seen[item.messageKey]); seen[item.messageKey] = true; } }); }
  visit(config); assert.deepStrictEqual(Object.keys(seen).sort(), schema.names.filter(function(name) { return name !== 'DisconnectAlert' && name !== 'WeatherEffects'; }).sort());
  assert(pkg.pebble.capabilities.indexOf('configurable') >= 0);
  var max = schema.copy(schema.defaults); max.WorldLabel = '12345678'; max.EventLabel = '1234567890'; max.EventDate = '2199-12-31';
  var payload = schema.wire(max, null, true);
  assert(schema.bytes(payload) < 600); assert(!payload[K.LocationName]); assert(!payload[K.WorldZone]);
});
test('sanitization clamps values, preserves missing/invalid keys, and validates calendar dates', function() {
  var old = schema.sanitize({WorldLabel: 'OLD', EventDate: '2024-02-29', Row1: 5});
  var s = schema.sanitize({Row1: 100, StepGoal: -5, WeatherRefresh: 9999, AnimationSpeed: 20,
    RotateSeconds: 1, CharacterActivity: 12, BackgroundColor: '#00ff55',
    ReducedMotion: 'false', WeatherEnabled: {value: 'true'}, WorldLabel: 'J\u0000a東京pan123456',
    EventDate: '2025-02-29', WorldZone: 'Bad/Zone', WeatherDetail: NaN}, old);
  assert.strictEqual(s.Row1, 6); assert.strictEqual(s.StepGoal, 100); assert.strictEqual(s.WeatherRefresh, 180);
  assert.strictEqual(s.AnimationSpeed, 500); assert.strictEqual(s.RotateSeconds, 15);
  assert.strictEqual(s.CharacterActivity, 7); assert.strictEqual(s.BackgroundColor, 0x00ff55);
  assert.strictEqual(s.ReducedMotion, false); assert.strictEqual(s.WeatherEnabled, true);
  assert.strictEqual(s.WorldLabel, 'Japan123'); assert.strictEqual(s.EventDate, '2024-02-29');
  assert.strictEqual(s.WorldZone, 'Asia/Tokyo'); assert.strictEqual(s.WeatherDetail, old.WeatherDetail);
  assert.strictEqual(schema.sanitize({2: {value: 4}}, old).Row1, 4);
  assert(!schema.validDate('2100-02-29')); assert(schema.validDate('2000-02-29'));
  assert(!schema.validDate('2026-04-31')); assert(!schema.validDate('0001-01-01'));
  var retired = schema.sanitize({DisconnectAlert: true, WeatherEffects: true});
  assert.strictEqual(retired.DisconnectAlert, false); assert.strictEqual(retired.WeatherEffects, false);
  assert(!/connection|disconnect|weather effects/i.test(JSON.stringify(config)));
  assert.strictEqual(schema.defaults.Theme, 1);
  assert.strictEqual(schema.sanitize({Theme: 4}).Theme, 1);
  assert.strictEqual(schema.sanitize({Theme: 0}).Theme, 0);
  assert.strictEqual(schema.sanitize({Preset: 3, AnimationMode: 1, Language: 4}).Preset, 0);
  assert.strictEqual(schema.sanitize({Language: 4}).Language, 4);
});
test('retired Lively becomes Arcade, and the form offers only the remaining modes', function() {
  assert.strictEqual(schema.defaults.AnimationMode, 1);
  assert.strictEqual(schema.sanitize({AnimationMode: 0}).AnimationMode, 1);
  assert.strictEqual(schema.sanitize({AnimationMode: 3}).AnimationMode, 4); // Once per minute becomes Still
  [1, 2, 4].forEach(function(mode) { assert.strictEqual(schema.sanitize({AnimationMode: mode}).AnimationMode, mode); });
  var found = [];
  (function visit(items) { items.forEach(function(item) { if (item.items) { visit(item.items); }
    else if (item.messageKey === 'AnimationMode') { found = item.options.map(function(o) { return o.value + ':' + o.label; }); } }); })(config);
  assert.deepStrictEqual(found, ['1:Arcade', '2:Relaxed', '4:Still']);
  assert.strictEqual(formHarness({AnimationMode: 0}).items.AnimationMode.get(), 0); // an unsanitized 0 never reaches the form
});

test('pair rows: companions sanitize, order is kept, and sections follow what the rows show', function() {
  assert.deepStrictEqual([schema.names[48], schema.names[49]], ['Row1Then', 'Row2Then']);
  assert.strictEqual(schema.keys.Row1Then, 49); assert.strictEqual(schema.keys.Row2Then, 50);
  assert.strictEqual(schema.defaults.Row1Then, 0); assert.strictEqual(schema.defaults.Row2Then, 0);
  [0, 2, 3, 4].forEach(function(v) { assert.strictEqual(schema.sanitize({Row2Then: v}).Row2Then, v); });
  [1, 5, 6, -3].forEach(function(v) { assert.strictEqual(schema.sanitize({Row1Then: v}).Row1Then, 0); });
  assert.strictEqual(schema.sanitize({Row2Then: 'x'}).Row2Then, 0);
  assert.strictEqual(schema.wire(schema.sanitize({Row2: 3, Row2Then: 2}), schema.defaults)[K.Row2Then], 2);
  // Presets restore the layout, pairs included.
  assert.strictEqual(schema.preset(2).Row1Then, 0); assert.strictEqual(schema.preset(4).Row2Then, 0);
  var hidden = function(form, name) { return !!(form.items[name] || form.ids[name]).hidden; };
  var form = formHarness({Row1: 1, Row2: 2});
  // Default layout: Date and Battery show; Steps, Weather, World and Event stay out of the way.
  assert(!hidden(form, 'DateFormat') && !hidden(form, 'BatteryStyle') && !hidden(form, 'h-date') && !hidden(form, 'h-battery'));
  ['StepStyle', 'WeatherUnits', 'LocationName', 'WorldZone', 'EventDate', 'h-daily-steps', 'h-weather-optional', 'weather-credit', 'weather-status'].forEach(function(name) {
    assert(hidden(form, name), name + ' should be hidden');
  });
  assert(hidden(form, 'Row1Then')); // Date cannot share a line
  assert(!hidden(form, 'Row2Then')); // Battery can
  // The pair's second item brings its section in, and the order is the user's.
  form.items.Row2Then.set(3);
  assert(!hidden(form, 'WeatherUnits') && !hidden(form, 'h-weather-optional') && !hidden(form, 'weather-credit'));
  assert(!hidden(form, 'BatteryStyle')); // the main item is still Battery
  form.items.Row2.set(3); form.items.Row2Then.set(2); // Weather first, then Battery
  assert(!hidden(form, 'WeatherUnits') && !hidden(form, 'BatteryStyle'));
  form.items.Row2.set(4); form.items.Row2Then.set(4); // an item never pairs with itself
  assert(!hidden(form, 'StepStyle') && hidden(form, 'WeatherUnits') && hidden(form, 'BatteryStyle'));
  form.items.Row2.set(1); // Date cannot pair: its companion control hides and is ignored
  assert(hidden(form, 'Row2Then') && hidden(form, 'StepStyle') && !hidden(form, 'DateFormat'));
  form.items.Row2.set(5); assert(!hidden(form, 'WorldZone') && hidden(form, 'EventDate'));
  form.items.Row2.set(6); assert(!hidden(form, 'EventDate') && hidden(form, 'WorldZone'));
  // Weather consent never hides: turned on, its section stays even with no weather row.
  var consent = formHarness({Row1: 1, Row2: 2, WeatherEnabled: true});
  assert(!hidden(consent, 'WeatherUnits') && !hidden(consent, 'h-weather-optional'));
  // Hidden controls keep their saved values.
  var kept = formHarness({Row1: 1, Row2: 2, StepStyle: 2, EventDate: '2027-01-01', WorldLabel: 'NYC'});
  assert(hidden(kept, 'StepStyle') && kept.items.StepStyle.get() === 2 && kept.items.EventDate.get() === '2027-01-01');
  // Applying a preset updates what shows.
  var traveller = formHarness({Row1: 1, Row2: 2}); traveller.items.Preset.set(4);
  assert(!hidden(traveller, 'WorldZone') && hidden(traveller, 'BatteryStyle') && !hidden(traveller, 'DateFormat'));
  var classic = formHarness({Row1: 3, Row2: 4}); classic.items.Preset.set(1);
  assert(hidden(classic, 'WeatherUnits') && hidden(classic, 'StepStyle') && hidden(classic, 'DateFormat'));
  // Every hidden control is real: nothing in a group is missing from the page.
  var seen = {};
  (function visit(list) { list.forEach(function(c) { if (c.items) { visit(c.items); } else { seen[c.id || c.messageKey] = true; } }); })(config);
  schema.groups(config).forEach(function(group) { group.targets.forEach(function(name) { assert(seen[name], name); }); });
});

test('preset form changes are visible before save and preserve opt-in/personal choices', function() {
  var form = formHarness({WeatherEnabled: true, WorldZone: 'Europe/London', StepGoal: 12345, WorldLabel: 'MUM', Row1: 6, AnimationMode: 1, AnimationSpeed: 750, AnimationPause: 4, CharacterActivity: 5});
  assert.strictEqual(form.items.Row1.get(), 6); // opening never reapplies saved Everyday
  form.items.Preset.set(4);
  assert.strictEqual(form.items.Theme.get(), 1);
  assert.strictEqual(form.items.Row1.get(), 5); assert.strictEqual(form.items.Row2.get(), 1);
  assert.strictEqual(form.items.AnimationMode.get(), 1); assert.strictEqual(form.items.Preset.get(), 4);
  assert.strictEqual(form.items.WorldLabel.get(), 'MUM'); assert.strictEqual(form.items.StepGoal.get(), 12345);
  assert.strictEqual(form.items.WeatherEnabled.get(), true);
  form.items.Row2.set(6); assert.strictEqual(form.items.Preset.get(), 0);
  form.items.Preset.set(5); assert.strictEqual(form.items.LargeTime.get(), true);
  assert.strictEqual(form.items.ReducedMotion.get(), false); assert.strictEqual(form.items.AnimationMode.get(), 1);
  form.items.Preset.set(1); assert.strictEqual(form.items.Row1.get(), 0); assert.strictEqual(form.items.Row2.get(), 0);
  assert.strictEqual(form.items.AnimationMode.get(), 1);
  assert.strictEqual(form.items.AnimationSpeed.get(), 750);
  assert.strictEqual(form.items.AnimationPause.get(), 4);
  assert.strictEqual(form.items.CharacterActivity.get(), 5);
  form.items.AnimationMode.set(2);
  assert.strictEqual(form.items.Preset.get(), 1); // Activity is independent of Classic.
  form.items.ReducedMotion.set(true); form.items.Preset.set(2);
  assert.strictEqual(form.items.ReducedMotion.get(), true);
  assert.deepStrictEqual(form.items.Preset.config.options.map(function(o) { return o.value; }), ['0','1','2','4','5']);
  var disabled = formHarness(); disabled.items.Preset.set(4);
  assert.strictEqual(disabled.items.WeatherEnabled.get(), false); assert(disabled.items.LocationName.disabled);
  disabled.items.WeatherEnabled.set(true); disabled.items.LocationMode.set(1); assert(!disabled.items.LocationName.disabled);
  disabled.items.CustomColors.set(true); assert(!disabled.items.BackgroundColor.disabled);
});
test('queue serializes, coalesces newer settings on failure, and ignores late callbacks', function() {
  var clock = new Clock(), pebble = new Pebble(); pebble.autoAck = false;
  var delivered = [], q = new Queue(pebble, timers(clock), null, function(tag, p) { delivered.push(p); });
  q.add('settings', {2: 3}); q.add('weather', {106: 0}); q.add('settings', {3: 4}); q.add('settings', {2: 5});
  assert.strictEqual(pebble.sent.length, 1); pebble.sent[0].bad(); clock.advance(1);
  assert.deepStrictEqual(pebble.sent[1].payload, {2: 5, 3: 4});
  pebble.sent[0].ok(); assert.strictEqual(delivered.length, 0);
  pebble.sent[1].ok(); assert.strictEqual(pebble.sent.length, 3); pebble.sent[2].ok();
  assert.strictEqual(delivered.length, 2); assert.strictEqual(q.active, null);
});
test('queue has bounded retries and recovers after an acknowledgement timeout', function() {
  var clock = new Clock(), pebble = new Pebble(); pebble.autoAck = false;
  var q = new Queue(pebble, timers(clock)); q.add('weather', {106: 2});
  clock.advance(10); assert(q.retryTimer); clock.advance(1);
  assert.strictEqual(pebble.sent.length, 2); pebble.sent[1].ok(); assert.strictEqual(q.active, null);
  q.add('settings', {2: 1});
  for (var i = 0; i < 6; i++) {
    pebble.sent[pebble.sent.length - 1].bad(); clock.advance(32);
  }
  assert.strictEqual(q.active, null); assert.strictEqual(q.pending.length, 0);
  assert.throws(function() { q.add('settings', {28: new Array(1000).join('X')}); }, /budget/);
});
test('weather opt-in prevents all GPS and network access', function() {
  var h = weatherHarness(); h.service.request(); h.service.request(); h.clock.advance(3600);
  assert.strictEqual(h.gps.length, 0); assert.strictEqual(h.network.calls.length, 0);
  assert.strictEqual(h.sent[h.sent.length - 1][K.WeatherStatus], 0);
});
test('manual geocoding + forecast use HTTPS, correct fields, units and cache rate limit', function() {
  var h = weatherHarness({WeatherEnabled: true, LocationMode: 1, LocationName: 'Paris, France'});
  h.service.request(); assert(h.network.calls[0].url.indexOf('https://geocoding-api.open-meteo.com/') === 0);
  assert(h.network.calls[0].url.indexOf('Paris%2C%20France') > 0);
  h.network.calls[0].done(null, {results: [{latitude: 48.8566, longitude: 2.3522, name: 'Paris'}]});
  assert(h.network.calls[1].url.indexOf('https://api.open-meteo.com/') === 0);
  assert(h.network.calls[1].url.indexOf('timezone=auto') > 0); assert(h.network.calls[1].url.indexOf('temperature_unit=celsius') > 0);
  h.network.calls[1].done(null, data(h.clock, 12.4));
  var result = h.sent[h.sent.length - 1]; assert.strictEqual(result[K.WeatherTemp], 12);
  assert.strictEqual(result[K.WeatherHigh], 16); assert.strictEqual(result[K.WeatherLow], 7);
  assert.strictEqual(result[K.WeatherCode], 61); assert.strictEqual(result[K.WeatherStatus], 1);
  assert.strictEqual(result[K.WeatherUpdated], h.clock.now() - 120);
  for (var i = 0; i < 10; i++) { h.service.request(); }
  assert.strictEqual(h.network.calls.length, 2);
  h.service.changed(schema.sanitize({WeatherUnits: 1}, h.service.settings));
  result = h.sent[h.sent.length - 1]; assert.strictEqual(result[K.WeatherTemp], 54);
  assert.strictEqual(result[K.WeatherUnit], 1); assert.strictEqual(h.network.calls.length, 2);
});
test('automatic location has bounded age, no high accuracy, and a timeout fallback', function() {
  var h = weatherHarness({WeatherEnabled: true}); h.service.request();
  assert.strictEqual(h.gps.length, 1); assert.strictEqual(h.gps[0].options.enableHighAccuracy, false);
  h.gps[0].ok({coords: {latitude: 35.68, longitude: 139.76}});
  assert.strictEqual(h.network.calls.length, 1); h.network.calls[0].done(null, data(h.clock, 20));
  assert.strictEqual(h.sent[h.sent.length - 1][K.WeatherLocation], 'LOCAL');
  var failed = weatherHarness({WeatherEnabled: true}); failed.service.request(); failed.clock.advance(15);
  assert.strictEqual(failed.network.calls.length, 0); assert.strictEqual(failed.service.busy, false);
  assert.strictEqual(failed.sent[failed.sent.length - 1][K.WeatherStatus], 2);
});
test('weather failures keep last good values/timestamp and never pretend to be fresh', function() {
  var h = weatherHarness({WeatherEnabled: true, LocationMode: 1, LocationName: 'Tokyo'});
  h.service.request(); h.network.calls[0].done(null, {results: [{latitude: 35, longitude: 139, name: 'Tokyo'}]});
  h.network.calls[1].done(null, data(h.clock, 18)); var updated = h.service.cache.updated;
  h.clock.advance(3600); assert.strictEqual(h.network.calls.length, 3); h.network.calls[2].done('network');
  var result = h.sent[h.sent.length - 1]; assert.strictEqual(result[K.WeatherStatus], 2);
  assert.strictEqual(result[K.WeatherUpdated], updated); assert.strictEqual(result[K.WeatherTemp], 18);
  h.service.request(); h.clock.advance(299); assert.strictEqual(h.network.calls.length, 3);
  h.clock.advance(1); assert.strictEqual(h.network.calls.length, 4);
  var invalid = data(h.clock, 21); invalid.current.weather_code = 200;
  h.network.calls[3].done(null, invalid); assert.strictEqual(h.service.cache.updated, updated);
});
test('location changes discard old responses and impose a global attempt interval', function() {
  var h = weatherHarness({WeatherEnabled: true, LocationMode: 1, LocationName: 'Tokyo'});
  h.service.request(); var old = h.network.calls[0];
  h.service.changed(schema.sanitize({LocationName: 'London'}, h.service.settings));
  assert(old.cancelled); assert.strictEqual(h.network.calls.length, 1);
  old.done(null, {results: [{latitude: 35, longitude: 139, name: 'Tokyo'}]}); assert.strictEqual(h.network.calls.length, 1);
  h.clock.advance(60); assert.strictEqual(h.network.calls.length, 2);
  h.network.calls[1].done(null, {results: [{latitude: 51, longitude: 0, name: 'London'}]});
  var forecast = h.network.calls[2];
  h.service.changed(schema.sanitize({WeatherEnabled: false}, h.service.settings));
  forecast.done(null, data(h.clock, 99)); assert.strictEqual(h.service.cache, null);
  assert.strictEqual(h.sent[h.sent.length - 1][K.WeatherStatus], 0);
});
test('cache survives phone reload, rejects corrupt cache and preserves explicit units', function() {
  var backend = new Backend(), h = weatherHarness({WeatherEnabled: true, LocationMode: 1, LocationName: 'Tokyo'}, backend);
  h.service.request(); h.network.calls[0].done(null, {results: [{latitude: 35, longitude: 139, name: 'Tokyo'}]});
  h.network.calls[1].done(null, data(h.clock, 0));
  var reload = weatherHarness({WeatherEnabled: true, WeatherUnits: 1, LocationMode: 1, LocationName: 'Tokyo'}, backend);
  reload.service.request(); assert.strictEqual(reload.network.calls.length, 0);
  assert.strictEqual(reload.sent[0][K.WeatherTemp], 32); assert.strictEqual(reload.sent[0][K.WeatherUnit], 1);
  backend.values['popeye-clock-weather-v1'] = '{bad json';
  reload = weatherHarness({WeatherEnabled: true}, backend); assert.strictEqual(reload.service.cache, null);
});
test('IANA offsets handle DST transitions, fractional zones, southern hemisphere and missing Intl', function() {
  function at(iso) { return Date.parse(iso) / 1000; }
  assert.strictEqual(world.offset('Asia/Tokyo', at('2026-01-01T00:00:00Z')), 540);
  assert.strictEqual(world.offset('Asia/Kathmandu', at('2026-01-01T00:00:00Z')), 345);
  assert.strictEqual(world.offset('Europe/London', at('2026-03-29T00:59:59Z')), 0);
  assert.strictEqual(world.offset('Europe/London', at('2026-03-29T01:00:00Z')), 60);
  assert.strictEqual(world.offset('America/New_York', at('2026-11-01T05:59:59Z')), -240);
  assert.strictEqual(world.offset('America/New_York', at('2026-11-01T06:00:00Z')), -300);
  assert.strictEqual(world.offset('Australia/Sydney', at('2026-01-01T00:00:00Z')), 660);
  assert.strictEqual(world.offset('Australia/Sydney', at('2026-07-01T00:00:00Z')), 600);
  assert.strictEqual(world.offset('Pacific/Chatham', at('2026-01-01T00:00:00Z')), 825);
  assert.strictEqual(world.offset('Bad/Zone', at('2026-01-01T00:00:00Z')), null);
  assert.strictEqual(world.offset('Asia/Tokyo', world.end), null);
  var previous = global.Intl; global.Intl = undefined;
  try { assert.strictEqual(world.offset('Europe/London', at('2026-07-01T00:00:00Z')), 60); }
  finally { global.Intl = previous; }
});
test('world service refreshes at a DST boundary and immediately on zone change', function() {
  var clock = new Clock(); clock.ms = Date.parse('2026-03-29T00:59:30Z'); var sent = [];
  var service = new world.World({now: clock.now.bind(clock), timers: timers(clock), zone: 'Europe/London',
    send: function(tag, payload) { sent.push(payload); }});
  service.refresh(); assert.strictEqual(sent[0][K.WorldOffset], 0);
  clock.advance(31); assert.strictEqual(sent[1][K.WorldOffset], 60);
  service.refresh('Asia/Tokyo'); assert.strictEqual(sent[2][K.WorldOffset], 540);
  assert.strictEqual(sent[2][K.WorldUpdated], clock.now());
  clock.ms = world.end * 1000; service.refresh(); assert.strictEqual(sent[3][K.WorldUpdated], 0);
});
test('HTTP parses once, handles non-2xx/malformed JSON and aborts after timeout', function() {
  var clock = new Clock(), requests = [], results = [];
  var get = Http(function() {
    var xhr = {open: function(method, url) { this.url = url; }, send: function() {}, abort: function() { this.aborted = true; }};
    requests.push(xhr); return xhr;
  }, timers(clock));
  function callback(error, value) { results.push([error, value]); }
  get('https://api.open-meteo.com/v1/forecast', callback);
  requests[0].status = 200; requests[0].responseText = '{"ok":true}'; requests[0].onload(); requests[0].onerror();
  assert.strictEqual(results.length, 1); assert.deepStrictEqual(results[0], [null, {ok: true}]);
  get('https://api.open-meteo.com/v1/forecast', callback); requests[1].status = 429; requests[1].onload();
  assert.strictEqual(results[1][0], 'http');
  get('https://api.open-meteo.com/v1/forecast', callback); requests[2].status = 200; requests[2].responseText = '{'; requests[2].onload();
  assert.strictEqual(results[2][0], 'json');
  get('https://api.open-meteo.com/v1/forecast', callback); clock.advance(15);
  assert(requests[3].aborted); assert.strictEqual(results[3][0], 'timeout');
  get('http://api.open-meteo.com/v1/forecast', callback); assert.strictEqual(requests.length, 4);
});
test('actual Clay opens an offline data URI and settings save uses small watch-only batches', function() {
  var h = appHarness(); h.pebble.emit('ready');
  assert(h.pebble.sent.some(function(m) { return m.payload[K.PhoneReady] === 1; }));
  assert.strictEqual(h.network.calls.length, 0); h.pebble.emit('showConfiguration');
  assert(h.pebble.urls[0].indexOf('data:text/html;charset=utf-8,') === 0);
  var html = decodeURIComponent(h.pebble.urls[0].split(',').slice(1).join(','));
  assert(html.indexOf('Popeye G&W Clock') >= 0); assert(html.indexOf('Open-Meteo') >= 0);
  assert(!/\blet (value|integerValue|currentValue) = /.test(html));
  var previous = h.pebble.sent.length;
  var response = {Row1: {value: 6}, WorldZone: {value: 'Europe/London'}, WorldLabel: {value: 'AB\u0000CD東京EFGHI'}};
  h.pebble.emit('webviewclosed', {response: encodeURIComponent(JSON.stringify(response))});
  var messages = h.pebble.sent.slice(previous), settings = messages.filter(function(m) { return m.payload[K.ConfigVersion]; });
  assert.strictEqual(settings.length, 1); assert.strictEqual(settings[0].payload[K.Row1], 6);
  assert.strictEqual(settings[0].payload[K.WorldLabel], 'ABCDEFGH'); assert(!settings[0].payload[K.Preset]);
  assert(!settings[0].payload[K.WorldZone]); assert(!settings[0].payload[K.LocationName]);
  assert(schema.bytes(settings[0].payload) < 100);
  assert(messages.some(function(m) { return m.payload[K.WorldOffset] === 60; }));
  assert.strictEqual(h.app.getSettings().WorldZone, 'Europe/London');
  assert.strictEqual(JSON.parse(h.backend.getItem('clay-settings')).WorldLabel, 'ABCDEFGH');
  var before = h.pebble.sent.length; h.pebble.emit('webviewclosed', {response: ''});
  h.pebble.emit('webviewclosed', {response: 'CANCELLED'}); h.pebble.emit('webviewclosed', {response: '%invalid'});
  assert.strictEqual(h.pebble.sent.length, before);
});
test('versioned preferences handle corrupt/unsupported storage and partial saves', function() {
  var backend = new Backend(); backend.setItem('popeye-clock-settings-v1', '{broken');
  var h = appHarness(backend); assert.strictEqual(h.app.getSettings().Row1, 1);
  h.pebble.emit('ready'); h.pebble.emit('webviewclosed', {response: JSON.stringify({StepGoal: {value: 12000}})});
  h = appHarness(backend); assert.strictEqual(h.app.getSettings().StepGoal, 12000);
  h.pebble.emit('webviewclosed', {response: JSON.stringify({StepStyle: {value: 2}})});
  assert.strictEqual(h.app.getSettings().StepGoal, 12000); assert.strictEqual(h.app.getSettings().Row2, 2);
  backend.setItem('popeye-clock-settings-v1', JSON.stringify({version: 999, settings: {Row1: 5}}));
  assert.strictEqual(appHarness(backend).app.getSettings().Row1, 1);
  var throwing = {getItem: function() { throw new Error('unavailable'); }, setItem: function() { throw new Error('full'); }};
  h = appHarness(throwing); h.pebble.emit('ready');
  h.pebble.emit('webviewclosed', {response: JSON.stringify({Row1: {value: 4}})});
  assert.strictEqual(h.app.getSettings().Row1, 4);
  assert(h.pebble.sent.some(function(m) { return m.payload[K.Row1] === 4; }));
});
test('later save resends unacknowledged preferences after retry exhaustion', function() {
  var h = appHarness(); h.pebble.emit('ready'); h.pebble.autoAck = false;
  h.pebble.emit('webviewclosed', {response: JSON.stringify({Row1: {value: 6}})});
  for (var i = 0; i < 6; i++) { h.pebble.sent[h.pebble.sent.length - 1].bad(); h.clock.advance(32); }
  // Empty data messages queued after the save may also be active; acknowledge them.
  while (h.app.queue.active) { h.pebble.sent[h.pebble.sent.length - 1].ok(); }
  h.pebble.autoAck = true; var previous = h.pebble.sent.length;
  h.pebble.emit('webviewclosed', {response: JSON.stringify({StepStyle: {value: 2}})});
  var payload = h.pebble.sent.slice(previous).filter(function(m) { return m.payload[K.ConfigVersion]; })[0].payload;
  assert.strictEqual(payload[K.Row1], 6); assert.strictEqual(payload[K.StepStyle], 2);
});
test('Clay compatibility preserves the emulator return route and old integer helpers', function() {
  var h = appHarness(); h.pebble.platform = 'pypkjs';
  h.pebble.emit('ready'); h.pebble.emit('showConfiguration');
  var url = h.pebble.urls[0];
  assert(url.indexOf('http://clay.pebble.com.s3-website-us-west-2.amazonaws.com/#') === 0);
  var html = decodeURIComponent(url.substr(url.indexOf('#') + 1));
  assert(html.indexOf('RETURN_TO') >= 0);
  var original = h.app.clay.generateUrl();
  var originalHtml = decodeURIComponent(original.substr(original.indexOf('#') + 1));
  assert.strictEqual(html, originalHtml.replace(/\blet (value|integerValue|currentValue) = /g, 'var $1 = '));
  assert(!/\blet (value|integerValue|currentValue) = /.test(html));
  var saved = {parseInt: Number.parseInt, isNaN: Number.isNaN, trunc: Math.trunc};
  try {
    Number.parseInt = undefined; Number.isNaN = undefined; Math.trunc = undefined;
    formHarness(); assert.strictEqual(Number.parseInt('12', 10), 12);
    assert(Number.isNaN(NaN)); assert(!Number.isNaN('abc'));
    assert.strictEqual(Math.trunc(-1.5), -1); assert.strictEqual(Math.trunc(1.5), 1);
  } finally { Number.parseInt = saved.parseInt; Number.isNaN = saved.isNaN; Math.trunc = saved.trunc; }
});
test('units changed during forecast are applied to the reply and unsafe labels cannot close Clay scripts', function() {
  var h = weatherHarness({WeatherEnabled: true, LocationMode: 1, LocationName: 'Tokyo'});
  h.service.request(); h.network.calls[0].done(null, {results: [{latitude: 35, longitude: 139, name: 'Tokyo'}]});
  h.service.changed(schema.sanitize({WeatherUnits: 1}, h.service.settings));
  h.network.calls[1].done(null, data(h.clock, 10));
  var payload = h.sent[h.sent.length - 1];
  assert.strictEqual(payload[K.WeatherTemp], 50); assert.strictEqual(payload[K.WeatherUnit], 1);
  assert.strictEqual(h.network.calls.length, 2);
  var s = schema.sanitize({EventLabel: '</script>', WorldLabel: '<HOME>', LocationName: '<Tokyo>'});
  assert.strictEqual(s.EventLabel, '/script'); assert.strictEqual(s.WorldLabel, 'HOME'); assert.strictEqual(s.LocationName, 'Tokyo');
});

console.log('Passed ' + tests + ' offline phone/configuration tests.');
