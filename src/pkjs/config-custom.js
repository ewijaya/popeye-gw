'use strict';
module.exports = function() {
  if (!Number.parseInt) { Number.parseInt = parseInt; }
  if (!Number.isNaN) { Number.isNaN = function(value) { return value !== value; }; }
  if (!Math.trunc) { Math.trunc = function(value) { return value < 0 ? Math.ceil(value) : Math.floor(value); }; }
  var clay = this;
  clay.on(clay.EVENTS.AFTER_BUILD, function() {
    var values = clay.meta.userData.values || {};
    Object.keys(values).forEach(function(key) { clay.getItemByMessageKey(key).set(values[key]); });
    var orientation = clay.getItemByMessageKey('Orientation');
    var buttons = clay.getItemByMessageKey('Buttons');
    function update() { if (+orientation.get() === 1) { buttons.show(); } else { buttons.hide(); } }
    orientation.on('change', update);
    update();
  });
};
