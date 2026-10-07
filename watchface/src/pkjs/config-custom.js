'use strict';

// Clay serializes this function into its offline HTML. All dependencies are
// supplied as userData, so this function deliberately captures no module state.
module.exports = function() {
  // Current Clay's integer controls use these methods; older config WebViews
  // may lack them even though the bundled code itself uses ES5 syntax.
  if (!Number.parseInt) { Number.parseInt = parseInt; }
  if (!Number.isNaN) { Number.isNaN = function(value) { return value !== value; }; }
  if (!Math.trunc) { Math.trunc = function(value) { return value < 0 ? Math.ceil(value) : Math.floor(value); }; }
  var clay = this;
  clay.on(clay.EVENTS.AFTER_BUILD, function() {
    var data = clay.meta.userData || {}, updating = false;
    var preset = clay.getItemByMessageKey('Preset');
    function item(name) { return clay.getItemByMessageKey(name); }
    function enabled(names, yes) {
      names.forEach(function(name) {
        var control = item(name);
        if (control) { if (yes) { control.enable(); } else { control.disable(); } }
      });
    }
    function dependencies() {
      var weather = !!item('WeatherEnabled').get();
      enabled(['WeatherUnits', 'WeatherDetail', 'WeatherRefresh', 'LocationMode'], weather);
      enabled(['LocationName'], weather && +item('LocationMode').get() === 1);
      enabled(['BackgroundColor', 'SegmentColor', 'AccentColor'], !!item('CustomColors').get());
      enabled(['QuietStart', 'QuietEnd'], !!item('QuietHours').get());
    }
    function applyPreset() {
      var id = +preset.get(), values = data.presets && data.presets[id];
      if (!values || !id) { return; }
      updating = true;
      Object.keys(values).forEach(function(name) { if (item(name)) { item(name).set(values[name]); } });
      updating = false;
      dependencies();
    }
    preset.on('change', applyPreset);
    var button = clay.getItemById('apply-preset');
    if (button) { button.on('click', applyPreset); }
    (data.presentation || []).forEach(function(name) {
      if (item(name)) {
        item(name).on('change', function() {
          if (!updating) { preset.set('0'); }
          dependencies();
        });
      }
    });
    ['WeatherEnabled', 'LocationMode', 'CustomColors', 'QuietHours'].forEach(function(name) {
      item(name).on('change', dependencies);
    });
    var status = clay.getItemById('weather-status');
    if (status && data.weatherStatus) { status.set(data.weatherStatus); }
    // Do not reapply a saved preset on page open: it would overwrite edits.
    dependencies();
  });
};
