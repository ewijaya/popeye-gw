'use strict';
function select(key, choices) {
  return {type: 'select', messageKey: key, label: key, defaultValue: '0', serializeValueAs: 'integer',
    options: choices.map(function(label, value) { return {label: label, value: String(value)}; })};
}
function toggle(key) { return {type: 'toggle', messageKey: key, label: key, defaultValue: false}; }
module.exports = [
  {type: 'heading', defaultValue: 'Popeye G&W'},
  {type: 'section', items: [
    select('Orientation', ['Vertical', 'Horizontal']),
    select('Buttons', ['Top', 'Bottom']),
    toggle('Swap'), toggle('Vibrate'),
    select('Sound', ['Off', 'Low', 'Medium', 'High']),
    select('Light', ['Auto', 'In play']),
    toggle('Ghosts'), select('Theme', ['Classic', 'Ivory']),
    toggle('Demo'), toggle('Online')
  ]},
  {type: 'submit', defaultValue: 'Save'}
];
