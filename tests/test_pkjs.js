/* Phone-side leaderboard code (src/js/pebble-js-app.js) with a mocked Pebble, XHR and storage.
 * Run by tests/test_pkjs.py; needs a working `node` (the SDK build itself needs none). */
'use strict';
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

/* Objects built inside the vm context have another prototype: compare their JSON. */
const same = (actual, expected, message) => assert.strictEqual(JSON.stringify(actual), JSON.stringify(expected), message);

const source = fs.readFileSync(path.join(__dirname, '..', 'src', 'js', 'pebble-js-app.js'), 'utf8');

function load(url, stored) {
  const sent = [];
  const requests = [];
  let handler = null;
  class XHR {
    constructor() { this.headers = {}; requests.push(this); }
    open(method, target) { this.method = method; this.url = target; }
    setRequestHeader(name, value) { this.headers[name] = value; }
    send(body) { this.body = body; }
    answer(status, text) { this.status = status; this.responseText = text; this.onload(); }
  }
  const context = {
    Pebble: {
      addEventListener(name, callback) { if (name === 'appmessage') handler = callback; },
      sendAppMessage(message) { sent.push(message); },
      getAccountToken() { return context.token; },
    },
    XMLHttpRequest: XHR,
    localStorage: { getItem() { if (stored === 'throw') throw new Error('blocked'); return stored === undefined ? null : stored; } },
    token: 'abcdef0123456789abcdef0123456789',
    JSON, Math, module: undefined,
  };
  vm.createContext(context);
  vm.runInContext(source + '\n;this.api = { toBase64, defaultName, playerName, collect };', context);
  if (url) vm.runInContext('LEADERBOARD_URL = ' + JSON.stringify(url), context);
  const send = (payload) => handler({ payload });
  return { context, sent, requests, send, api: context.api };
}

// Base64 matches the reference encoder for every length and byte value.
{
  const { api } = load('');
  for (let length = 0; length < 70; length++) {
    const bytes = Array.from({ length }, (_, i) => (i * 37 + length) & 255);
    assert.strictEqual(api.toBase64(bytes), Buffer.from(bytes).toString('base64'), 'length ' + length);
  }
  const all = Array.from({ length: 256 }, (_, i) => i);
  assert.strictEqual(api.toBase64(all), Buffer.from(all).toString('base64'));
}

// Default and stored nicknames satisfy the server's name rule and never reveal the token.
{
  const { api } = load('');
  const rule = /^[A-Za-z0-9 _-]{1,12}$/;
  const token = 'abcdef0123456789abcdef0123456789';
  const name = api.defaultName(token);
  assert.ok(rule.test(name) && /^Sailor[0-9A-Z]{4}$/.test(name), name);
  assert.strictEqual(api.defaultName(token), name);
  assert.notStrictEqual(api.defaultName('0123456789abcdef0123456789abcdef'), name);
  assert.ok(!name.toLowerCase().includes(token.slice(0, 4)));
  for (let i = 0; i < 200; i++) assert.ok(rule.test(api.defaultName('token-' + i * 7919)));
  assert.strictEqual(load('', 'Olive').api.playerName(token), 'Olive');
  assert.strictEqual(load('', 'bad name!!').api.playerName(token), name);
  assert.strictEqual(load('', 'x'.repeat(13)).api.playerName(token), name);
  assert.strictEqual(load('', 'throw').api.playerName(token), name);
}

// Chunk assembly: in order, a restart on seq 0, and every broken sequence is dropped.
{
  const { api } = load('');
  const data = Array.from({ length: 600 }, (_, i) => i & 255);
  const chunk = (seq, from, to, length) => ({ LB_SEQ: seq, LB_LEN: length === undefined ? 600 : length, LB_DATA: data.slice(from, to) });
  assert.strictEqual(api.collect(chunk(0, 0, 256)), null);
  assert.strictEqual(api.collect(chunk(1, 256, 512)), null);
  same(api.collect(chunk(2, 512, 600)), data);
  assert.strictEqual(api.collect(chunk(1, 256, 512)), null); // no upload in progress
  api.collect(chunk(0, 0, 256));
  assert.strictEqual(api.collect(chunk(2, 512, 600)), null); // a chunk went missing
  assert.strictEqual(api.collect(chunk(1, 256, 512)), null); // and the upload was dropped
  api.collect(chunk(0, 0, 256));
  assert.strictEqual(api.collect(chunk(1, 256, 512, 601)), null); // length changed
  same(api.collect(chunk(0, 0, 100, 100)), data.slice(0, 100)); // single chunk
  assert.strictEqual(api.collect(chunk(0, 0, 300, 100)), null); // more data than announced
  assert.strictEqual(api.collect(chunk(0, 0, 256, 4096)), null); // over the replay size limit
  assert.strictEqual(api.collect({ LB_SEQ: 0, LB_LEN: 5 }), null);
  assert.strictEqual(api.collect({ LB_SEQ: 0, LB_LEN: 5, LB_DATA: [] }), null);
}

// Unconfigured (the shipped state): no network, one "not configured" answer.
{
  const run = load('');
  run.send({ LB_SEQ: 0, LB_LEN: 10, LB_DATA: [1, 2, 3, 4, 5] });
  run.send({ LB_SEQ: 1, LB_LEN: 10, LB_DATA: [6, 7, 8, 9, 10] });
  run.send({ unrelated: 1 });
  assert.strictEqual(run.requests.length, 0);
  same(run.sent, [{ LB_STATUS: 3 }]);
}

// Configured: one POST with the base64 replay, the account token and a valid name.
function submit(run, parts) {
  parts.forEach((data, seq) => run.send({ LB_SEQ: seq, LB_LEN: parts.reduce((n, p) => n + p.length, 0), LB_DATA: data }));
}
{
  const run = load('https://board.example.org');
  submit(run, [[1, 2, 3, 4, 5, 6, 7], [8, 9, 10]]);
  assert.strictEqual(run.requests.length, 1);
  const request = run.requests[0];
  assert.strictEqual(request.method, 'POST');
  assert.strictEqual(request.url, 'https://board.example.org/v1/daily');
  assert.strictEqual(request.headers['Content-Type'], 'application/json');
  assert.ok(request.timeout > 0);
  const body = JSON.parse(request.body);
  same(Object.keys(body).sort(), ['name', 'player', 'replay']);
  assert.strictEqual(body.player, run.context.token);
  assert.strictEqual(body.replay, Buffer.from([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]).toString('base64'));
  assert.ok(/^[A-Za-z0-9 _-]{1,12}$/.test(body.name));
  assert.strictEqual(run.sent.length, 0); // nothing until the server answers
  request.answer(200, '{"rank":12,"total":140,"best":55}');
  same(run.sent, [{ LB_STATUS: 1, LB_RANK: 12, LB_TOTAL: 140 }]);
  request.onerror(); request.ontimeout(); // a late event does not answer twice
  assert.strictEqual(run.sent.length, 1);
}

// Failures: refusals are final, everything else is "try later", and a bad answer is not a rank.
for (const [status, text, expected] of [
  [422, '{"error":"replay_rejected"}', 2], [400, '{"error":"bad_name"}', 2], [413, '{}', 2],
  [429, '{"error":"rate_limited"}', 0], [500, '{"error":"server_error"}', 0], [503, '', 0],
  [200, 'not json', 0], [200, '{"rank":0,"total":5}', 0], [200, '{"rank":9,"total":5}', 0], [200, '{}', 0],
]) {
  const run = load('https://board.example.org');
  submit(run, [[1, 2, 3]]);
  run.requests[0].answer(status, text);
  same(run.sent, [{ LB_STATUS: expected }], status + ' ' + text);
}
for (const event of ['onerror', 'ontimeout']) {
  const run = load('https://board.example.org');
  submit(run, [[1, 2, 3]]);
  run.requests[0][event]();
  same(run.sent, [{ LB_STATUS: 0 }], event);
}

// No account token: nothing is posted, and the watch is told to try later.
for (const token of ['', undefined]) {
  const run = load('https://board.example.org');
  run.context.token = token;
  submit(run, [[1, 2, 3]]);
  assert.strictEqual(run.requests.length, 0);
  same(run.sent, [{ LB_STATUS: 0 }]);
}

// A broken upload never posts.
{
  const run = load('https://board.example.org');
  run.send({ LB_SEQ: 1, LB_LEN: 10, LB_DATA: [1, 2, 3, 4, 5] });
  run.send({ LB_SEQ: 0, LB_LEN: 10, LB_DATA: [1, 2, 3, 4, 5] });
  run.send({ LB_SEQ: 2, LB_LEN: 10, LB_DATA: [6, 7, 8, 9, 10] });
  assert.strictEqual(run.requests.length, 0);
}

// The shipped constant is empty.
assert.ok(/^var LEADERBOARD_URL = '';/m.test(source), 'LEADERBOARD_URL must ship empty');
console.log('pkjs tests passed');
