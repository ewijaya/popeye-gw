'use strict';
// Reproducible, development-only generation. The phone never downloads zone data.
// Run with the repository's verified Node and the pinned npm dependencies.
var moment = require('moment-timezone/moment-timezone-utils');
var zones = require('../src/pkjs/zones').cities;
var fs = require('fs');
var subset = {
  version: moment.tz.dataVersion,
  start: Date.UTC(2020, 0, 1) / 1000,
  end: Date.UTC(2041, 0, 1) / 1000,
  zones: zones.map(function(city) {
    return moment.tz.pack(moment.tz.filterYears(moment.tz.zone(city[1]), 2020, 2040));
  }),
  links: []
};
fs.writeFileSync(require('path').join(__dirname, '../src/pkjs/timezone-data.json'), JSON.stringify(subset, null, 2) + '\n');
console.log('Bundled ' + subset.zones.length + ' IANA ' + subset.version + ' zones, 2020–2040.');
