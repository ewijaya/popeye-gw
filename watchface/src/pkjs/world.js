'use strict';
var moment = require('moment-timezone/moment-timezone');
var data = require('./timezone-data.json');
moment.tz.load(data);
var keys = require('./schema').keys;

function offset(zoneName, seconds) {
  if (seconds < data.start || seconds >= data.end) { return null; }
  var zone = moment.tz.zone(zoneName);
  if (!zone || !require('./zones').has(zoneName)) { return null; }
  var minutesEast = -zone.utcOffset(seconds * 1000);
  return isFinite(minutesEast) && minutesEast >= -840 && minutesEast <= 840 ? Math.round(minutesEast) + 0 : null;
}
function World(options) {
  this.now = options.now; this.timers = options.timers; this.send = options.send;
  this.zone = options.zone; this.timer = null;
}
World.prototype.refresh = function(zoneName) {
  var self = this;
  if (zoneName) { self.zone = zoneName; }
  if (self.timer) { self.timers.clearTimeout(self.timer); self.timer = null; }
  var now = self.now(), minutes = offset(self.zone, now), payload = {};
  // Outside the bundled range, report unavailable rather than a fabricated offset.
  payload[keys.WorldUpdated] = minutes === null ? 0 : now;
  if (minutes !== null) { payload[keys.WorldOffset] = minutes; }
  self.send('world', payload);
  var next = now + 86400, zone = moment.tz.zone(self.zone);
  if (zone && minutes !== null) {
    for (var i = 0; i < zone.untils.length; i++) {
      var transition = zone.untils[i] / 1000;
      if (transition > now) { next = Math.min(next, transition + 1); break; }
    }
    next = Math.min(next, data.end);
  }
  self.timer = self.timers.setTimeout(function() { self.timer = null; self.refresh(); },
    Math.max(1000, (next - now) * 1000));
};
module.exports = {World: World, offset: offset, start: data.start, end: data.end, dataVersion: data.version};
