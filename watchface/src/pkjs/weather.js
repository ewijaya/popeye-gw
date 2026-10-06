'use strict';
var schema = require('./schema');
var K = schema.keys;
var CACHE_KEY = 'popeye-clock-weather-v1';
var LOCATION_KEY = 'popeye-clock-location-v1';
var ATTEMPT_KEY = 'popeye-clock-weather-attempt-v1';
var MIN_ATTEMPT = 60;
var ERROR_RETRY = 300;
var WMO = [0, 1, 2, 3, 45, 48, 51, 53, 55, 56, 57, 61, 63, 65, 66, 67,
  71, 73, 75, 77, 80, 81, 82, 85, 86, 95, 96, 99];
function profile(settings) {
  return settings.LocationMode + ':' + (settings.LocationMode ? settings.LocationName : 'phone');
}
function number(value, min, max) {
  return typeof value === 'number' && isFinite(value) && value >= min && value <= max;
}
function validLocation(value, p, now) {
  return value && value.version === 1 && value.profile === p &&
    number(value.latitude, -90, 90) && number(value.longitude, -180, 180) &&
    number(value.updated, 1, now + 60) && typeof value.label === 'string';
}
function validCache(value, now) {
  return value && value.version === 1 && typeof value.profile === 'string' &&
    number(value.temp, -150, 100) && number(value.high, -150, 100) &&
    number(value.low, -150, 100) && value.low <= value.high &&
    WMO.indexOf(value.code) !== -1 && number(value.updated, 1, now + 600) &&
    number(value.fetched, 1, now + 60) && typeof value.label === 'string';
}
function Weather(options) {
  this.get = options.get; this.geolocation = options.geolocation;
  this.storage = options.storage; this.timers = options.timers; this.now = options.now;
  this.send = options.send; this.settings = options.settings;
  this.timer = null; this.cancel = null; this.gpsTimer = null; this.busy = false;
  this.generation = 0; this.error = false;
  var now = this.now();
  var cache = this.storage.read(CACHE_KEY);
  this.cache = validCache(cache, now) ? cache : null;
  var attempt = this.storage.read(ATTEMPT_KEY);
  this.lastAttempt = attempt && attempt.version === 1 && number(attempt.time, 1, now + 60) ? attempt.time : 0;
}
Weather.prototype.currentCache = function() {
  return this.cache && this.cache.profile === profile(this.settings) ? this.cache : null;
};
Weather.prototype.publish = function(forceError) {
  var payload = {}, cache = this.currentCache(), now = this.now(), unit = this.settings.WeatherUnits;
  payload[K.WeatherUnit] = unit;
  if (!this.settings.WeatherEnabled) { payload[K.WeatherStatus] = 0; this.send('weather', payload); return; }
  var fresh = cache && now - cache.updated <= this.settings.WeatherRefresh * 60;
  payload[K.WeatherStatus] = cache ? (forceError || !fresh ? 2 : 1) : (forceError ? 2 : 0);
  payload[K.WeatherUpdated] = cache ? cache.updated : 0;
  if (cache) {
    function temp(c) { return Math.round(unit === 1 ? c * 9 / 5 + 32 : c); }
    payload[K.WeatherTemp] = temp(cache.temp);
    payload[K.WeatherHigh] = temp(cache.high);
    payload[K.WeatherLow] = temp(cache.low);
    payload[K.WeatherCode] = cache.code;
    payload[K.WeatherLocation] = schema.ascii(cache.label, 16, 'WEATHER') || 'WEATHER';
  }
  this.send('weather', payload);
};
Weather.prototype.stop = function() {
  this.generation++;
  if (this.timer) { this.timers.clearTimeout(this.timer); this.timer = null; }
  if (this.gpsTimer) { this.timers.clearTimeout(this.gpsTimer); this.gpsTimer = null; }
  if (this.cancel) { this.cancel(); this.cancel = null; }
  this.busy = false;
};
Weather.prototype.changed = function(settings) {
  var changedLocation = profile(settings) !== profile(this.settings);
  var enabledChanged = settings.WeatherEnabled !== this.settings.WeatherEnabled;
  if (changedLocation || enabledChanged) { this.stop(); this.error = false; }
  this.settings = settings;
  this.request();
};
Weather.prototype.schedule = function(seconds) {
  var self = this;
  if (self.timer) { self.timers.clearTimeout(self.timer); self.timer = null; }
  if (!self.settings.WeatherEnabled) { return; }
  self.timer = self.timers.setTimeout(function() { self.timer = null; self.request(); },
    Math.max(1000, seconds * 1000));
};
Weather.prototype.request = function() {
  var self = this, now = self.now(), cache = self.currentCache();
  self.publish(self.error);
  if (!self.settings.WeatherEnabled || self.busy) { return; }
  var due = cache && !self.error ? cache.fetched + self.settings.WeatherRefresh * 60 : 0;
  if (self.lastAttempt) { due = Math.max(due, self.lastAttempt + (self.error ? ERROR_RETRY : MIN_ATTEMPT)); }
  if (due > now) { self.schedule(due - now); return; }
  self.lastAttempt = now;
  self.storage.write(ATTEMPT_KEY, {version: 1, time: now});
  self.busy = true;
  var generation = self.generation, p = profile(self.settings);
  function active() {
    return generation === self.generation && self.settings.WeatherEnabled && p === profile(self.settings);
  }
  function finish(error, cacheValue) {
    if (!active()) { return; }
    self.busy = false; self.cancel = null;
    self.error = !!error;
    if (!error) { self.cache = cacheValue; self.storage.write(CACHE_KEY, cacheValue); }
    self.publish(!!error);
    self.schedule(error ? ERROR_RETRY : self.settings.WeatherRefresh * 60);
  }
  self.location(p, function(error, location) {
    if (!active()) { return; }
    if (error) { finish(error); return; }
    var url = 'https://api.open-meteo.com/v1/forecast?latitude=' + location.latitude.toFixed(4) +
      '&longitude=' + location.longitude.toFixed(4) +
      '&current=temperature_2m,weather_code&daily=temperature_2m_max,temperature_2m_min' +
      '&temperature_unit=celsius&timezone=auto&timeformat=unixtime&forecast_days=1';
    self.cancel = self.get(url, function(networkError, data) {
      if (!active()) { return; }
      if (networkError) { finish(networkError); return; }
      var current = data && data.current, daily = data && data.daily, stamp = self.now();
      if (!current || !daily || !Array.isArray(daily.temperature_2m_max) ||
          !Array.isArray(daily.temperature_2m_min) ||
          !number(current.temperature_2m, -150, 100) || WMO.indexOf(current.weather_code) === -1 ||
          !number(daily.temperature_2m_max[0], -150, 100) || !number(daily.temperature_2m_min[0], -150, 100) ||
          daily.temperature_2m_min[0] > daily.temperature_2m_max[0] ||
          !number(current.time, stamp - 86400, stamp + 600)) {
        finish('response'); return;
      }
      finish(null, {version: 1, profile: p, temp: current.temperature_2m,
        high: daily.temperature_2m_max[0], low: daily.temperature_2m_min[0],
        code: current.weather_code, updated: Math.min(stamp, Math.floor(current.time)),
        fetched: stamp, label: location.label});
    });
  });
};
Weather.prototype.location = function(p, callback) {
  var self = this, now = self.now(), cached = self.storage.read(LOCATION_KEY);
  var maxAge = self.settings.LocationMode ? 7 * 86400 : 15 * 60;
  if (validLocation(cached, p, now) && now - cached.updated <= maxAge) {
    callback(null, cached); return;
  }
  var generation = self.generation;
  function save(latitude, longitude, label) {
    if (generation !== self.generation) { return; }
    if (!number(latitude, -90, 90) || !number(longitude, -180, 180)) { callback('location'); return; }
    var location = {version: 1, profile: p, latitude: latitude, longitude: longitude,
      label: schema.ascii(label, 16, 'WEATHER') || 'WEATHER', updated: self.now()};
    self.storage.write(LOCATION_KEY, location);
    callback(null, location);
  }
  if (self.settings.LocationMode === 1) {
    var city = self.settings.LocationName;
    if (city.length < 2) { callback('city'); return; }
    self.cancel = self.get('https://geocoding-api.open-meteo.com/v1/search?name=' +
      encodeURIComponent(city) + '&count=1&language=en&format=json', function(error, data) {
      if (generation !== self.generation) { return; }
      if (error || !data || !Array.isArray(data.results) || !data.results.length) { callback('city'); return; }
      var first = data.results[0];
      save(first.latitude, first.longitude, first.name);
    });
    return;
  }
  if (!self.geolocation || typeof self.geolocation.getCurrentPosition !== 'function') { callback('location'); return; }
  var ended = false;
  function gpsDone(error, position) {
    if (ended || generation !== self.generation) { return; }
    ended = true;
    if (self.gpsTimer) { self.timers.clearTimeout(self.gpsTimer); self.gpsTimer = null; }
    if (error || !position || !position.coords) { callback('location'); return; }
    save(position.coords.latitude, position.coords.longitude, 'LOCAL');
  }
  self.gpsTimer = self.timers.setTimeout(function() { gpsDone('timeout'); }, 15000);
  try {
    self.geolocation.getCurrentPosition(function(position) { gpsDone(null, position); },
      function() { gpsDone('location'); }, {enableHighAccuracy: false, timeout: 10000, maximumAge: 10 * 60 * 1000});
  } catch (e) { gpsDone('location'); }
};
Weather.prototype.statusText = function() {
  if (!this.settings.WeatherEnabled) { return 'Weather is disabled. Enable updates to use Open-Meteo.'; }
  var cache = this.currentCache();
  if (!cache) { return 'No weather yet. Check your city or allow phone location, then save.'; }
  var age = Math.max(0, Math.floor((this.now() - cache.updated) / 60));
  return 'Last weather update: ' + age + ' minutes ago' +
    (this.error || age > this.settings.WeatherRefresh ? ' (stale). Cached data stays visible.' : '.');
};
module.exports = Weather;
