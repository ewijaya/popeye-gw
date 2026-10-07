'use strict';
var schema = require('./schema');
var D = schema.defaults;
function select(key, label, options, description) {
  return {type: 'select', messageKey: key, label: label, defaultValue: String(D[key]),
    serializeValueAs: key === 'WorldZone' ? 'string' : 'integer',
    options: options.map(function(o, i) {
      return typeof o === 'string' ? {label: o, value: String(i)} : {label: o[0], value: String(o[1])};
    }), description: description || ''};
}
function toggle(key, label, description) {
  return {type: 'toggle', messageKey: key, label: label, defaultValue: D[key], description: description || ''};
}
function number(key, label, min, max, description) {
  return {type: 'input', messageKey: key, label: label, defaultValue: String(D[key]),
    serializeValueAs: 'integer', attributes: {type: 'number', min: min, max: max, step: 1},
    description: description || ''};
}
function input(key, label, max, description, type) {
  return {type: 'input', messageKey: key, label: label, defaultValue: D[key],
    attributes: {type: type || 'text', maxlength: max}, description: description || ''};
}
function color(key, label) {
  return {type: 'color', messageKey: key, label: label, defaultValue: D[key], sunlight: false};
}
function section(title, items) {
  return {type: 'section', items: [{type: 'heading', defaultValue: title}].concat(items)};
}
var rows = ['Hidden', 'Date', 'Battery', 'Weather', 'Steps', 'World time', 'Event'];
var formats = ['Follow watch', '12-hour', '24-hour'];
module.exports = [
  {type: 'heading', defaultValue: 'Popeye G&W Clock'},
  {type: 'text', defaultValue: 'Your pocket-sized LCD world. Settings work offline; weather updates need the phone and Internet.'},
  section('Start with a preset', [
    select('Preset', 'Preset', [['Custom', 0], ['Classic', 1], ['Everyday', 2], ['Traveller', 4], ['Large', 5]],
      'Presets change layout and style. Your animation activity and weather consent stay as you set them.'),
    {type: 'button', id: 'apply-preset', defaultValue: 'Apply selected preset again'},
    select('Row1', 'Top information row', rows), select('Row2', 'Second information row', rows),
    number('RotateSeconds', 'Rotate rows every (seconds)', 0, 120,
      '0 keeps both rows visible. 15–120 cycles your selected nonempty rows in one line.'),
    select('TimeFormat', 'Main clock', formats), toggle('LargeTime', 'Larger clock digits'),
    toggle('HighContrast', 'High contrast'),
    toggle('BlinkColon', 'Blink clock colon', 'Off gives a steady colon and avoids second ticks.')
  ]),
  section('Date', [
    select('DateFormat', 'Date order', ['Day – month', 'Month – day', 'Numeric']),
    select('Language', 'Day / month language', ['English', 'Deutsch', 'Français', 'Español', '日本語'],
      'Japanese uses month/day order and compact weekday kanji.'),
    toggle('ShowYear', 'Show year'), toggle('ShowWeek', 'Show ISO week number')
  ]),
  section('Battery', [
    select('BatteryStyle', 'Battery display', ['Percentage only', 'Battery icon only', 'Battery icon + percentage', 'Spinach can + percentage']),
    number('BatteryThreshold', 'Show battery only below (%)', 0, 100, '0 always shows the battery row.')
  ]),
  section('Daily steps', [
    number('StepGoal', 'Daily step goal', 100, 100000),
    select('StepStyle', 'Step display', ['Step count', 'Goal percentage', 'Progress meter']),
    toggle('Celebrate', 'Celebrate reaching the goal', 'A brief spinach celebration once per day when motion is enabled.')
  ]),
  section('Weather — optional', [
    toggle('WeatherEnabled', 'Enable weather updates',
      'Uses Open-Meteo. Automatic mode shares phone coordinates with Open-Meteo; manual mode searches your city instead.'),
    select('WeatherUnits', 'Temperature units', ['Celsius (°C)', 'Fahrenheit (°F)']),
    select('WeatherDetail', 'Weather display', ['Current temperature', 'Today’s high / low']),
    number('WeatherRefresh', 'Refresh every (minutes)', 15, 180),
    select('LocationMode', 'Weather location', ['Phone location', 'Manual city']),
    input('LocationName', 'City or postal code', 80,
      'For example Tokyo, JP or Paris, France. Add a country or region to distinguish cities.'),
    {type: 'text', id: 'weather-status', defaultValue: 'Cached weather stays visible with a stale marker if an update fails.'},
    {type: 'text', defaultValue: 'Weather: <a href="https://open-meteo.com/">Open-Meteo</a> (CC BY 4.0). Location search: GeoNames.'}
  ]),
  section('World clock', [
    select('WorldZone', 'City / timezone', require('./zones').cities,
      'Uses bundled IANA rules for daylight saving time; no Internet needed.'),
    input('WorldLabel', 'Clock label', 8, 'Up to 8 ASCII characters.'),
    select('WorldFormat', 'World clock format', ['Follow main clock', '12-hour', '24-hour'])
  ]),
  section('Event countdown', [
    input('EventLabel', 'Event name', 10, 'Up to 10 ASCII characters.'),
    input('EventDate', 'Event date', 10, 'Leave empty to disable. Date range 1900–2199.', 'date'),
    toggle('EventRepeat', 'Repeat every year', 'A February 29 anniversary uses February 28 in other years.'),
    toggle('EventElapsed', 'Count days after the event', 'When off, a past one-time event shows --.')
  ]),
  section('Animation activity — independent of preset', [
    select('AnimationMode', 'Animation activity', ['Lively', 'Arcade', 'Relaxed', 'Once per minute', 'Still'],
      'Works with every preset, including Classic. Changing presets keeps this choice.'),
    toggle('ReducedMotion', 'Reduce motion', 'Pauses character motion in every preset. Turn off to use your selected activity.'),
    number('AnimationSpeed', 'Animation beat (milliseconds)', 0, 4000,
      '0 uses the mode’s pace: Lively 1000, Arcade 600, Relaxed 2000. Custom range 500–4000.'),
    number('AnimationPause', 'Pause between loops (beats)', 0, 10),
    select('CharacterActivity', 'Active characters', [['None', 0], ['Olive', 1], ['Popeye', 2],
      ['Olive + Popeye', 3], ['Brutus', 4], ['Olive + Brutus', 5], ['Popeye + Brutus', 6], ['All three', 7]]),
    toggle('QuietHours', 'Pause motion during quiet hours'),
    number('QuietStart', 'Quiet hours start (hour)', 0, 23),
    number('QuietEnd', 'Quiet hours end (hour)', 0, 23,
      '24-hour clock. Matching start and end means no quiet interval.'),
    number('LowBatteryCutoff', 'Pause motion at / below battery (%)', 0, 50, '0 disables this limit.')
  ]),
  section('LCD style', [
    select('Theme', 'Theme', ['Original', 'Ivory', 'Green LCD', 'Amber']),
    toggle('CustomColors', 'Use custom colors'), color('BackgroundColor', 'Background'),
    color('SegmentColor', 'Active segments'), color('AccentColor', 'Accent'),
    select('GhostStrength', 'Inactive segment ghosts', ['Off', 'Faint', 'Strong']),
    toggle('ColorArtwork', 'Keep artwork in color', 'Off uses monochrome scenery.')
  ]),
  {type: 'submit', defaultValue: 'Save settings'}
];
