'use strict';
function Storage(backend) { this.backend = backend; this.memory = {}; }
Storage.prototype.read = function(key) {
  try {
    var value = this.backend && this.backend.getItem(key);
    if (value) { return JSON.parse(value); }
  } catch (e) { /* Corrupt storage or an unavailable backend: use safe defaults. */ }
  return this.memory[key] || null;
};
Storage.prototype.write = function(key, value) {
  this.memory[key] = value;
  try { if (this.backend) { this.backend.setItem(key, JSON.stringify(value)); } }
  catch (e) { /* Watch settings still apply when phone storage is full. */ }
};
module.exports = Storage;
