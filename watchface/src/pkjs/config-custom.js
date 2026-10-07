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
    function setVisible(control, yes) {
      if (control) { if (yes) { control.show(); } else { control.hide(); } }
    }
    function pairs(main) { return (data.pairable || []).indexOf(main) !== -1; }
    // A row shows an item itself, or as the second half of a pair.
    function uses(row) {
      return [1, 2].some(function(n) {
        var main = +item('Row' + n).get(), then = +item('Row' + n + 'Then').get();
        return main === row || (pairs(main) && pairs(then) && then !== main && then === row);
      });
    }
    function layout() {
      [1, 2].forEach(function(n) { setVisible(item('Row' + n + 'Then'), pairs(+item('Row' + n).get())); });
      (data.groups || []).forEach(function(group) {
        var yes = group.use.rows.some(uses) || !!(group.use.or && item(group.use.or).get());
        group.targets.forEach(function(name) { setVisible(item(name) || clay.getItemById(name), yes); });
      });
    }
    function dependencies() {
      layout();
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
