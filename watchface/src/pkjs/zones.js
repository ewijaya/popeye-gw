'use strict';
var cities = [
  ['Tokyo', 'Asia/Tokyo'], ['UTC', 'Etc/UTC'], ['London', 'Europe/London'],
  ['Paris / Berlin', 'Europe/Paris'], ['Helsinki', 'Europe/Helsinki'],
  ['Istanbul', 'Europe/Istanbul'], ['Dubai', 'Asia/Dubai'],
  ['Mumbai / New Delhi', 'Asia/Kolkata'], ['Kathmandu', 'Asia/Kathmandu'],
  ['Bangkok', 'Asia/Bangkok'], ['Jakarta', 'Asia/Jakarta'],
  ['Singapore / Hong Kong', 'Asia/Singapore'], ['Seoul', 'Asia/Seoul'],
  ['Sydney', 'Australia/Sydney'], ['Adelaide', 'Australia/Adelaide'],
  ['Perth', 'Australia/Perth'], ['Auckland', 'Pacific/Auckland'],
  ['Honolulu', 'Pacific/Honolulu'], ['Los Angeles', 'America/Los_Angeles'],
  ['Denver', 'America/Denver'], ['Phoenix', 'America/Phoenix'],
  ['Chicago', 'America/Chicago'], ['New York / Toronto', 'America/New_York'],
  ['Mexico City', 'America/Mexico_City'], ['Sao Paulo', 'America/Sao_Paulo'],
  ['Buenos Aires', 'America/Argentina/Buenos_Aires'], ['Johannesburg', 'Africa/Johannesburg'],
  ['Cairo', 'Africa/Cairo'], ['Chatham Islands', 'Pacific/Chatham']
];
module.exports = {
  cities: cities,
  has: function(name) {
    return cities.some(function(city) { return city[1] === name; });
  },
  options: cities.map(function(city) { return {label: city[0], value: city[1]}; })
};
