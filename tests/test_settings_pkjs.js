'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const create = require('../src/pkjs/settings');
const config = require('../src/pkjs/config');
const custom = require('../src/pkjs/config-custom');
const keys = ['Orientation', 'Buttons', 'Swap', 'Vibrate', 'Sound', 'Light', 'Ghosts', 'Theme', 'Demo', 'Online'];
const current = [0, 1, 0, 1, 2, 0, 1, 0, 1, 0]; // Upgraded install: Light Auto, not the new-install default.
const plain = value => JSON.parse(JSON.stringify(value));
function harness(blockStorage) {
  const listeners = {}, sent = [], urls = [], jobs = new Map();
  let next = 0;
  const pebble = {
    addEventListener(name, fn) { (listeners[name] = listeners[name] || []).push(fn); },
    sendAppMessage(payload, ok) { sent.push(plain(payload)); ok(); },
    openURL(url) { urls.push(url); },
    getAccountToken() { return 'test-account'; },
    getWatchToken() { return 'test-watch'; },
    getActiveWatchInfo() { return {platform: 'emery', firmware: {major: 4}}; }
  };
  const stored = {'clay-settings': JSON.stringify({Light: '1', Sound: '3', Orientation: '1'})};
  const sandbox = {module: {exports: {}}, exports: {}, Pebble: pebble, console,
    localStorage: {getItem(key) { return stored[key] || null; },
      setItem(key, val) { if (blockStorage) throw Error('Read-only storage'); stored[key] = val; }},
    require(name) {
      assert.strictEqual(name, 'message_keys');
      return Object.fromEntries(require('../package.json').pebble.messageKeys.map((key, i) => [key, 10000 + i]));
    }};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../node_modules/@rebble/clay/src/js/index.js'), 'utf8'), sandbox);
  const app = create({Clay: sandbox.module.exports, pebble, now: () => 100000,
    timers: {setTimeout(fn) { jobs.set(++next, fn); return next; }, clearTimeout(id) { jobs.delete(id); }}});
  function emit(name, event) { (listeners[name] || []).forEach(fn => fn(event)); }
  function answer(values, status = 0, id = sent[sent.length - 1].CFG_ID) {
    emit('appmessage', {payload: {CFG_DATA: values, CFG_STATUS: status, CFG_ID: id}});
  }
  function tick() { const [id, fn] = jobs.entries().next().value; jobs.delete(id); fn(); }
  function open(values = current) { emit('showConfiguration'); answer(values); }
  function close(patch) { emit('webviewclosed', {response: encodeURIComponent(JSON.stringify(patch))}); }
  function html() { return decodeURIComponent(urls[urls.length - 1].split(',').slice(1).join(',')); }
  return {app, pebble, emit, answer, tick, open, close, html, sent, urls, jobs, stored};
}

// Exactly the watch menu, in order; no presets, alarm controls, or phone-only settings.
assert.deepStrictEqual(config[1].items.map(item => item.messageKey), keys);
assert.strictEqual(config.filter(item => item.type === 'submit').length, 1);
assert.deepStrictEqual(config[1].items[4].options.map(o => o.label), ['Off', 'Low', 'Medium', 'High']);
assert.deepStrictEqual(config[1].items[5].options.map(o => o.label), ['Auto', 'In play']);
{
  const h = harness();
  h.emit('ready');
  assert.strictEqual(h.sent[0].CFG_REQUEST, 1);
  assert(!h.sent[0].CFG_PATCH); // Old phone defaults never overwrite watch preferences.
  h.answer(current);
  assert.strictEqual(h.urls.length, 0);
  h.emit('showConfiguration');
  assert.strictEqual(h.urls.length, 0); // Await a fresh watch snapshot.
  h.answer(current, 0, -1); assert.strictEqual(h.urls.length, 0);
  h.answer(current.slice(1)); assert.strictEqual(h.urls.length, 0);
  h.answer(current);
  assert(h.urls[0].startsWith('data:text/html'));
  assert(h.html().includes('pebblejs://close#'));
  assert(!/\blet (value|integerValue|currentValue) = /.test(h.html()));
  assert.deepStrictEqual(plain(h.app.clay().meta.userData.values),
    {Orientation: '0', Buttons: '1', Swap: false, Vibrate: true, Sound: '2', Light: '0', Ghosts: true, Theme: '0', Demo: true, Online: false});
  // Only changed fields; tolerate Clay's wrapped values and string selections.
  h.close({Orientation: {value: '1'}, Light: '1', Sound: {value: 0}});
  const patch = h.sent[h.sent.length - 1].CFG_PATCH;
  assert.strictEqual(patch.length, 12);
  assert.strictEqual(patch[0], 1 | 16 | 32); assert.strictEqual(patch[1], 0);
  assert.strictEqual(patch[3], 1); // Hidden button preference remains Bottom.
  assert.strictEqual(patch[6], 0); assert.strictEqual(patch[7], 1);
  // Transport ACK has already arrived; persistence ACK is still required.
  assert.strictEqual(h.jobs.size, 1);
  h.tick(); assert.deepStrictEqual(h.sent[h.sent.length - 1], h.sent[h.sent.length - 2]);
  h.answer([1, 1, 0, 1, 0, 1, 1, 0, 1, 0]); assert.strictEqual(h.jobs.size, 0);
  // A later watch edit is fetched again on the next opening.
  h.open([0, 0, 1, 0, 3, 0, 0, 1, 0, 1]);
  assert.strictEqual(h.app.clay().meta.userData.values.Buttons, '0');
  assert.strictEqual(h.app.clay().meta.userData.values.Sound, '3');
}
for (const response of [undefined, '', 'CANCELLED', '%not-json', '[]']) {
  const h = harness(); h.open(); const count = h.sent.length;
  h.emit('webviewclosed', {response}); assert.strictEqual(h.sent.length, count);
}
{
  const h = harness(); h.open(); const count = h.sent.length;
  h.close(Object.fromEntries(keys.map((key, i) => [key, current[i]])));
  assert.strictEqual(h.sent.length, count); // No-op Save performs no flash write.
}
for (const invalid of [-1, 4, null, '2x', {}, 0.5]) {
  const h = harness(); h.open(); const count = h.sent.length;
  h.close({Sound: invalid}); assert.strictEqual(h.sent.length, count);
  assert(h.html().includes('Invalid settings'));
}
for (const status of [1, 2, 3]) {
  const h = harness(); h.open(); h.close({Light: 1}); h.answer(current, status);
  assert(h.html().includes(status === 2 ? 'Finish or quit' : 'could not save'));
  assert.strictEqual(h.jobs.size, 0);
}
{
  const h = harness(); h.emit('showConfiguration'); h.tick(); h.tick(); h.tick();
  assert.strictEqual(h.sent.length, 3); assert.strictEqual(h.jobs.size, 0);
  assert(h.html().includes('Open Popeye G'));
  assert(!h.html().includes('messageKey: "Orientation"'));
  h.open(); h.close({Light: 1}); h.tick(); h.tick(); h.tick();
  assert(h.html().includes('Could not confirm the save'));
}
// Actual custom function restores watch values even if Clay's cache cannot be written,
// hides Buttons only in Vertical, and does not reset that hidden preference.
{
  const h = harness(true); h.open();
  const items = {};
  keys.forEach(key => { items[key] = {value: 9, visible: true, listeners: {},
    get() { return this.value; }, set(v) { this.value = v; },
    show() { this.visible = true; }, hide() { this.visible = false; },
    on(name, fn) { this.listeners[name] = fn; }}; });
  const ctx = {meta: h.app.clay().meta, EVENTS: {AFTER_BUILD: 'build'},
    on(event, fn) { fn(); }, getItemByMessageKey(key) { return items[key]; }};
  custom.call(ctx);
  assert.strictEqual(items.Light.value, '0'); assert.strictEqual(items.Buttons.value, '1');
  assert.strictEqual(items.Buttons.visible, false);
  items.Orientation.set('1'); items.Orientation.listeners.change();
  assert.strictEqual(items.Buttons.visible, true); assert.strictEqual(items.Buttons.value, '1');
}
console.log('settings pkjs tests passed');
