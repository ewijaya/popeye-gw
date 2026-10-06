'use strict';
var schema = require('./schema');
function Queue(pebble, timers, log, onDelivered) {
  this.pebble = pebble; this.timers = timers; this.log = log || function() {};
  this.pending = []; this.active = null; this.retryTimer = null; this.ackTimer = null;
  this.onDelivered = onDelivered || function() {};
}
Queue.prototype.add = function(tag, payload) {
  if (!payload || !Object.keys(payload).length) { return; }
  if (schema.bytes(payload) > 900) { throw new Error('AppMessage exceeds safe 900-byte budget'); }
  var queued = null;
  for (var i = 0; i < this.pending.length; i++) {
    if (this.pending[i].tag === tag) { queued = this.pending[i]; break; }
  }
  if (queued) {
    queued.payload = tag === 'settings' ? merge(queued.payload, payload) : schema.copy(payload);
    queued.tries = 0;
  } else {
    this.pending.push({tag: tag, payload: schema.copy(payload), tries: 0});
  }
  this.pump();
};
function merge(a, b) {
  var result = schema.copy(a);
  Object.keys(b).forEach(function(k) { result[k] = b[k]; });
  return result;
}
Queue.prototype.pump = function() {
  var self = this;
  if (self.active || self.retryTimer || !self.pending.length) { return; }
  var message = self.pending.shift();
  self.active = message;
  var finished = false;
  function finish(ok) {
    if (finished) { return; }
    finished = true;
    if (self.ackTimer) { self.timers.clearTimeout(self.ackTimer); self.ackTimer = null; }
    self.active = null;
    if (ok) { self.onDelivered(message.tag, message.payload); self.pump(); return; }
    message.tries++;
    if (message.tries <= 5) {
      // Coalesce newer queued data into a failed message before retrying it.
      for (var i = 0; i < self.pending.length; i++) {
        if (self.pending[i].tag === message.tag) {
          message.payload = message.tag === 'settings' ? merge(message.payload, self.pending[i].payload) : self.pending[i].payload;
          self.pending.splice(i, 1); break;
        }
      }
      self.pending.unshift(message);
      self.retryTimer = self.timers.setTimeout(function() {
        self.retryTimer = null; self.pump();
      }, Math.min(30000, Math.pow(2, message.tries - 1) * 1000));
    } else {
      self.log('AppMessage delivery deferred until the next phone connection.');
      self.pump();
    }
  }
  self.ackTimer = self.timers.setTimeout(function() { finish(false); }, 10000);
  try { self.pebble.sendAppMessage(message.payload, function() { finish(true); }, function() { finish(false); }); }
  catch (e) { finish(false); }
};
module.exports = Queue;
