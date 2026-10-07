'use strict';

// ES5 throughout: PebbleKit JS on older mobile apps has no Promise or Intl.
var names = ['Preset', 'Row1', 'Row2', 'RotateSeconds', 'TimeFormat',
  'LargeTime', 'HighContrast', 'ReducedMotion', 'BlinkColon', 'DateFormat',
  'Language', 'ShowYear', 'ShowWeek', 'BatteryStyle', 'BatteryThreshold',
  'DisconnectAlert', 'StepGoal', 'StepStyle', 'Celebrate', 'WeatherEnabled',
  'WeatherUnits', 'WeatherDetail', 'WeatherRefresh', 'WeatherEffects',
  'LocationMode', 'LocationName', 'WorldZone', 'WorldLabel', 'WorldFormat',
  'EventLabel', 'EventDate', 'EventRepeat', 'EventElapsed', 'AnimationMode',
  'AnimationSpeed', 'AnimationPause', 'CharacterActivity', 'QuietStart',
  'QuietEnd', 'QuietHours', 'LowBatteryCutoff', 'Theme', 'BackgroundColor',
  'SegmentColor', 'AccentColor', 'CustomColors', 'GhostStrength', 'ColorArtwork'];
var keys = {};
names.forEach(function(name, i) { keys[name] = i + 1; });
['RequestData', 'WeatherTemp', 'WeatherHigh', 'WeatherLow', 'WeatherCode',
  'WeatherUpdated', 'WeatherStatus', 'WorldOffset', 'WorldUpdated',
  'ConfigVersion', 'PhoneReady', 'WeatherLocation', 'WeatherUnit']
  .forEach(function(name, i) { keys[name] = i + 100; });

var defaults = {
  Preset: 2, Row1: 1, Row2: 2, RotateSeconds: 0, TimeFormat: 0,
  LargeTime: false, HighContrast: false, ReducedMotion: false, BlinkColon: false,
  DateFormat: 0, Language: 0, ShowYear: false, ShowWeek: false,
  BatteryStyle: 2, BatteryThreshold: 0, DisconnectAlert: false,
  StepGoal: 10000, StepStyle: 0, Celebrate: true,
  WeatherEnabled: false, WeatherUnits: 0, WeatherDetail: 0, WeatherRefresh: 60,
  WeatherEffects: false, LocationMode: 0, LocationName: '', WorldZone: 'Asia/Tokyo',
  WorldLabel: 'HOME', WorldFormat: 0, EventLabel: 'EVENT', EventDate: '',
  EventRepeat: false, EventElapsed: false, AnimationMode: 1,
  AnimationSpeed: 0, AnimationPause: 1, CharacterActivity: 7,
  QuietStart: 22, QuietEnd: 7, QuietHours: false, LowBatteryCutoff: 20,
  Theme: 1, BackgroundColor: 0xFFFFFF, SegmentColor: 0, AccentColor: 0xFF5500,
  CustomColors: false, GhostStrength: 1, ColorArtwork: true
};
var bounds = {
  Preset: [0, 5], Row1: [0, 6], Row2: [0, 6], RotateSeconds: [0, 120],
  TimeFormat: [0, 2], DateFormat: [0, 2], Language: [0, 4], BatteryStyle: [0, 3],
  BatteryThreshold: [0, 100], StepGoal: [100, 100000], StepStyle: [0, 2],
  WeatherUnits: [0, 1], WeatherDetail: [0, 1], WeatherRefresh: [15, 180],
  LocationMode: [0, 1], WorldFormat: [0, 2], AnimationMode: [0, 4],
  AnimationSpeed: [0, 4000], AnimationPause: [0, 10], CharacterActivity: [0, 7],
  QuietStart: [0, 23], QuietEnd: [0, 23], LowBatteryCutoff: [0, 50],
  Theme: [0, 3], BackgroundColor: [0, 0xFFFFFF], SegmentColor: [0, 0xFFFFFF],
  AccentColor: [0, 0xFFFFFF], GhostStrength: [0, 2]
};
// Presets change layout/style, preserving independently selected animation.
var presetIds = [1, 2, 4, 5];
var presentation = ['Row1', 'Row2', 'RotateSeconds', 'TimeFormat', 'LargeTime',
  'HighContrast', 'BlinkColon', 'DateFormat', 'Language',
  'ShowYear', 'ShowWeek', 'BatteryStyle', 'BatteryThreshold', 'StepStyle',
  'WeatherDetail', 'WorldFormat', 'Theme',
  'BackgroundColor', 'SegmentColor', 'AccentColor', 'CustomColors',
  'GhostStrength', 'ColorArtwork'];

function own(obj, key) { return Object.prototype.hasOwnProperty.call(obj, key); }
function copy(obj) {
  var result = {};
  Object.keys(obj).forEach(function(k) { result[k] = obj[k]; });
  return result;
}
function unwrap(v) {
  return v && typeof v === 'object' && own(v, 'value') ? v.value : v;
}
function ascii(value, max, fallback) {
  if (typeof value !== 'string') { return fallback; }
  var s = value.replace(/[^\x20-\x7e]|[<>]/g, '').replace(/\s+/g, ' ').trim();
  return s.substr(0, max);
}
function validDate(value) {
  if (value === '') { return true; }
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) { return false; }
  var y = +value.substr(0, 4), m = +value.substr(5, 2), d = +value.substr(8, 2);
  if (y < 1900 || y > 2199 || m < 1 || m > 12 || d < 1) { return false; }
  var days = [31, (y % 4 === 0 && (y % 100 !== 0 || y % 400 === 0)) ? 29 : 28,
    31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
  return d <= days[m - 1];
}
function sanitize(patch, previous) {
  var result = copy(previous || defaults);
  patch = patch && typeof patch === 'object' && !Array.isArray(patch) ? patch : {};
  names.forEach(function(name) {
    var value = own(patch, name) ? patch[name] : patch[keys[name]];
    if (typeof value === 'undefined') { return; }
    value = unwrap(value);
    if (typeof defaults[name] === 'boolean') {
      if (value === true || value === 1 || value === '1' || value === 'true') { result[name] = true; }
      else if (value === false || value === 0 || value === '0' || value === 'false') { result[name] = false; }
    } else if (bounds[name]) {
      if (typeof value !== 'number' && typeof value !== 'string') { return; }
      if (value === '') { return; }
      if (/Color$/.test(name) && typeof value === 'string' && /^(#|0x)[0-9a-f]{6}$/i.test(value)) {
        value = parseInt(value.replace(/^(#|0x)/i, ''), 16);
      }
      value = Number(value);
      if (!isFinite(value)) { return; }
      value = Math.round(value);
      if (name === 'Theme' && (value < 0 || value > 3)) { value = defaults.Theme; }
      result[name] = Math.max(bounds[name][0], Math.min(bounds[name][1], value));
    } else if (name === 'LocationName') {
      // Unicode is useful to geocoding. Never forward the full city string to C.
      if (typeof value === 'string') {
        result[name] = value.replace(/[\x00-\x1f\x7f<>]/g, '').trim().substr(0, 80);
      }
    } else if (name === 'WorldZone') {
      if (typeof value === 'string' && require('./zones').has(value)) { result[name] = value; }
    } else if (name === 'WorldLabel' || name === 'EventLabel') {
      result[name] = ascii(value, name === 'WorldLabel' ? 8 : 10, result[name]);
    } else if (name === 'EventDate' && validDate(value)) { result[name] = value; }
  });
  if (result.RotateSeconds > 0 && result.RotateSeconds < 15) { result.RotateSeconds = 15; }
  if (result.AnimationSpeed > 0 && result.AnimationSpeed < 500) { result.AnimationSpeed = 500; }
  if (result.Theme < 0 || result.Theme > 3) { result.Theme = defaults.Theme; }
  if (result.Preset === 3) { result.Preset = 0; } // Preserve retired Active choices as Custom.
  if (result.AnimationMode === 0) { result.AnimationMode = 1; } // Retired Lively becomes Arcade.
  if (result.AnimationMode === 3) { result.AnimationMode = 4; } // Retired Once per minute becomes Still.
  // Retired controls retain their wire IDs for old saved settings and phones.
  result.DisconnectAlert = false; result.WeatherEffects = false;
  return result;
}
function preset(id) {
  var result = {};
  if (presetIds.indexOf(id) === -1) { return {Preset: 0}; }
  presentation.forEach(function(name) { result[name] = defaults[name]; });
  if (id === 1) { result.Row1 = 0; result.Row2 = 0; }
  if (id === 4) { result.Row1 = 5; result.Row2 = 1; }
  if (id === 5) {
    result.LargeTime = true; result.HighContrast = true;
  }
  result.Preset = id;
  return result;
}
function wire(settings, previous, includePreset) {
  var result = {}, changed = false;
  names.forEach(function(name) {
    if (name === 'LocationName' || name === 'WorldZone' || (name === 'Preset' && !includePreset)) { return; }
    if (!previous || settings[name] !== previous[name]) {
      result[keys[name]] = typeof settings[name] === 'boolean' ? (settings[name] ? 1 : 0) : settings[name];
      changed = true;
    }
  });
  if (changed) { result[keys.ConfigVersion] = 1; }
  return result;
}
function bytes(payload) {
  var size = 1;
  Object.keys(payload).forEach(function(k) {
    size += 7 + (typeof payload[k] === 'string' ? payload[k].length + 1 : 4);
  });
  return size;
}
module.exports = {version: 1, names: names, keys: keys, defaults: defaults, presetIds: presetIds,
  bounds: bounds, presentation: presentation, sanitize: sanitize, preset: preset,
  wire: wire, bytes: bytes, ascii: ascii, validDate: validDate, copy: copy};
