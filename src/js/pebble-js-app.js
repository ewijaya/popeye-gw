/* Popeye G&W phone side: posts the watch's best Daily replay to the leaderboard.
 *
 * Disabled by default. While LEADERBOARD_URL is empty this file sends nothing to the network; it
 * only tells the watch "not configured" (status 3) if the Online setting is on. Plain ES5 for the
 * phone's JavaScript engine, no libraries. The watch protocol is described in
 * src/c/online_net.h and the server in server/README.md. */
var LEADERBOARD_URL = ''; /* e.g. 'https://leaderboard.example.org' (no trailing slash) */

var REQUEST_TIMEOUT_MS = 15000;
var MAX_REPLAY_BYTES = 2048;

var STATUS_RETRY = 0;
var STATUS_OK = 1;
var STATUS_REJECTED = 2;
var STATUS_UNAVAILABLE = 3;

var upload = null; /* {length, bytes: [], next} while chunks arrive */

/* Base64 without btoa, which PebbleKit JS does not promise. */
function toBase64(bytes) {
  var alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
  var out = '';
  var i;
  for (i = 0; i < bytes.length; i += 3) {
    var a = bytes[i];
    var b = i + 1 < bytes.length ? bytes[i + 1] : 0;
    var c = i + 2 < bytes.length ? bytes[i + 2] : 0;
    out += alphabet.charAt(a >> 2);
    out += alphabet.charAt(((a & 3) << 4) | (b >> 4));
    out += i + 1 < bytes.length ? alphabet.charAt(((b & 15) << 2) | (c >> 6)) : '=';
    out += i + 2 < bytes.length ? alphabet.charAt(c & 63) : '=';
  }
  return out;
}

/* "Sailor" plus four characters derived from the account token, so the default names nobody. */
function defaultName(token) {
  var hash = 5381;
  var name = '';
  var digits = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ';
  var i;
  for (i = 0; i < token.length; i++) {
    hash = ((hash * 33) ^ token.charCodeAt(i)) >>> 0;
  }
  for (i = 0; i < 4; i++) {
    name += digits.charAt(hash % 36);
    hash = Math.floor(hash / 36);
  }
  return 'Sailor' + name;
}

/* A stored nickname must match the server's rule, otherwise the default is used. */
function playerName(token) {
  var stored = null;
  try {
    stored = localStorage.getItem('nickname');
  } catch (e) {
    stored = null;
  }
  return stored && /^[A-Za-z0-9 _-]{1,12}$/.test(stored) ? stored : defaultName(token);
}

function reply(status, rank, total) {
  var message = { 'LB_STATUS': status };
  if (status === STATUS_OK) {
    message['LB_RANK'] = rank;
    message['LB_TOTAL'] = total;
  }
  Pebble.sendAppMessage(message, function () {}, function () {});
}

function post(replay) {
  var token = '';
  var request;
  var done = false;
  function finish(status, rank, total) {
    if (done) {
      return;
    }
    done = true;
    reply(status, rank, total);
  }
  try {
    token = Pebble.getAccountToken();
  } catch (e) {
    token = '';
  }
  if (!token) {
    finish(STATUS_RETRY);
    return;
  }
  request = new XMLHttpRequest();
  request.open('POST', LEADERBOARD_URL + '/v1/daily', true);
  request.timeout = REQUEST_TIMEOUT_MS;
  request.setRequestHeader('Content-Type', 'application/json');
  request.onload = function () {
    var body = null;
    if (request.status === 200) {
      try {
        body = JSON.parse(request.responseText);
      } catch (e) {
        body = null;
      }
      if (body && body.rank >= 1 && body.total >= body.rank) {
        finish(STATUS_OK, body.rank, body.total);
      } else {
        finish(STATUS_RETRY);
      }
    } else if (request.status === 400 || request.status === 413 || request.status === 422) {
      finish(STATUS_REJECTED); /* The server refused this replay; asking again will not help. */
    } else {
      finish(STATUS_RETRY);
    }
  };
  request.onerror = function () { finish(STATUS_RETRY); };
  request.ontimeout = function () { finish(STATUS_RETRY); };
  try {
    request.send(JSON.stringify({ player: token, name: playerName(token), replay: toBase64(replay) }));
  } catch (e) {
    finish(STATUS_RETRY);
  }
}

/* Collects the watch's chunks in order; returns the whole replay when the last one arrives. */
function collect(payload) {
  var seq = payload['LB_SEQ'];
  var length = payload['LB_LEN'];
  var data = payload['LB_DATA'];
  var i;
  if (typeof seq !== 'number' || typeof length !== 'number' || !data || !data.length) {
    upload = null;
    return null;
  }
  if (seq === 0) {
    upload = { length: length, bytes: [], next: 0 };
  }
  if (!upload || upload.next !== seq || upload.length !== length || length > MAX_REPLAY_BYTES) {
    upload = null;
    return null;
  }
  for (i = 0; i < data.length; i++) {
    upload.bytes.push(data[i] & 255);
  }
  upload.next++;
  if (upload.bytes.length > length) {
    upload = null;
    return null;
  }
  if (upload.bytes.length === length) {
    var whole = upload.bytes;
    upload = null;
    return whole;
  }
  return null;
}

Pebble.addEventListener('appmessage', function (event) {
  var replay;
  if (!event.payload || event.payload['LB_SEQ'] === undefined) {
    return;
  }
  if (!LEADERBOARD_URL) {
    if (event.payload['LB_SEQ'] === 0) {
      reply(STATUS_UNAVAILABLE); /* Not configured: one answer, no network traffic at all. */
    }
    return;
  }
  replay = collect(event.payload);
  if (replay) {
    post(replay);
  }
});

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { toBase64: toBase64, defaultName: defaultName, playerName: playerName, collect: collect };
}
