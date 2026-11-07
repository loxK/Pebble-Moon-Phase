/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Checks of the plugin as the phone app runs it: the bundle of src/plugin/moon.js
 * in a context of its own, with the host's registerSourceHandler and the app's
 * localStorage. Plain Node: `node tests/plugin.test.js` exits non-zero on a
 * failure.
 */

var assert = require('assert');
var path = require('path');
var vm = require('vm');
var root = path.join(__dirname, '..');
var Moon = require(path.join(root, 'src', 'pkjs', 'moon_phase.js'));
var bundle = require(path.join(root, 'tools', 'bundle_plugin.js')).bundle;
var manifest = require(path.join(root, 'package.json')).pebble.plugin;

var failures = 0;

function test(name, body) {
  try {
    body();
    console.log('ok   ' + name);
  } catch (e) {
    failures++;
    console.log('FAIL ' + name + '\n     ' + e.message);
  }
}

var SCRIPT = bundle(root, 'src/plugin/moon.js');

/** Ask the plugin for moon/`item` at `iso`, the app having stored `store`. */
function read(item, iso, store, language) {
  var handler = null;
  var context = vm.createContext({
    Pebble: { registerSourceHandler: function (fn) { handler = fn; } },
    localStorage: { getItem: function (k) { return k in store ? store[k] : null; } },
    navigator: { language: language || 'fr-FR' },
  });
  vm.runInContext('Date.now = function () { return ' + Date.parse(iso) + '; };', context);
  vm.runInContext(SCRIPT, context);
  var reply = null;
  handler({ kind: 'source', category: 'moon', item: item }, {
    data: function (envelope) { reply = { data: envelope }; },
    error: function (code, message) { reply = { error: code, message: message }; },
  });
  // From the plugin's context, so deepEqual compares plain values.
  return JSON.parse(JSON.stringify(reply));
}

function source(item) {
  return manifest.sources.filter(function (s) { return s.items.indexOf(item) >= 0; })[0];
}

/** Each property the instance carries is declared, in its declared shapes only. */
function checkShapes(instance, declared, required) {
  Object.keys(instance.properties).forEach(function (name) {
    assert.ok(declared[name], name + ' is not declared');
    Object.keys(instance.properties[name]).forEach(function (shape) {
      assert.ok(declared[name].indexOf(shape) >= 0, name + '.' + shape + ' is not declared');
    });
  });
  required.forEach(function (name) {
    declared[name].forEach(function (shape) {
      assert.ok(instance.properties[name] && instance.properties[name][shape],
                instance.instanceId + ' lacks ' + name + '.' + shape);
    });
  });
}

var NOW = '2026-11-07T03:00:00Z';
var SOUTH = { hemi: '1', clock24h: '1' };

test('moon/now carries every declared property, in its shapes', function () {
  var declared = source('now').properties;
  var reply = read('now', NOW, SOUTH);
  assert.strictEqual(reply.data.instances.length, 1);
  checkShapes(reply.data.instances[0], declared, Object.keys(declared));
});

test('moon/now agrees with the engine', function () {
  var p = read('now', NOW, SOUTH).data.instances[0].properties;
  var t = Date.parse(NOW) / 1000;
  var fraction = Moon.lunationFraction(t);
  assert.strictEqual(p.waxing.boolean.value, fraction < 0.5);
  assert.strictEqual(p.lunation.numericValue.value, fraction);
  assert.strictEqual(p.distance.numericValue.value, Math.round(Moon.distanceKm(t)));
  assert.strictEqual(p.approaching.boolean.value, Moon.isApproaching(t));
  assert.strictEqual(p.next_phase.timestamp.value, Moon.upcoming(t, 1)[0].time);
  // Two days before the 2026-11-09 new moon.
  assert.ok(p.age.numericValue.value > 27 && p.age.numericValue.value < 28, 'age ' + p.age.numericValue.value);
});

test('moon/now turns ascending and descending around with the hemisphere', function () {
  var south = read('now', NOW, SOUTH).data.instances[0].properties.ascending;
  var north = read('now', NOW, { hemi: '0' }).data.instances[0].properties.ascending;
  assert.strictEqual(south.boolean.value, !north.boolean.value);
  assert.strictEqual(south.shortText.text, 'Montante');
  assert.strictEqual(north.shortText.text, 'Descendante');
});

test('moon/event lists the next 30 days soonest first, and the next eclipse', function () {
  var declared = source('event').properties;
  var instances = read('event', NOW, SOUTH).data.instances;
  var t = Date.parse(NOW) / 1000;
  var kinds = {};
  instances.forEach(function (instance, i) {
    checkShapes(instance, declared, ['kind', 'name', 'time']);
    var time = instance.properties.time.timestamp.value;
    assert.ok(time > t, instance.instanceId + ' is not ahead');
    if (i > 0) assert.ok(time >= instances[i - 1].properties.time.timestamp.value, 'order');
    kinds[instance.properties.kind.longText.text] = true;
  });
  ['new_moon', 'first_quarter', 'full_moon', 'last_quarter', 'perigee', 'apogee',
   'ascending_node', 'descending_node', 'highest', 'lowest', 'lunar_eclipse'].forEach(function (k) {
    assert.ok(kinds[k], 'no ' + k);
  });
  var eclipse = instances[instances.length - 1];
  assert.strictEqual(eclipse.properties.kind.longText.text, 'lunar_eclipse');
  assert.strictEqual(new Date(eclipse.properties.time.timestamp.value * 1000).toISOString().slice(0, 10),
                     '2028-01-12');
  assert.strictEqual(eclipse.properties.shadow.numericValue.value, 6);
});

test('moon/event names the highest pass from the hemisphere', function () {
  function first(store) {
    return read('event', NOW, store).data.instances.filter(function (i) {
      return /highest|lowest/.test(i.properties.kind.longText.text);
    })[0];
  }
  var south = first(SOUTH), north = first({ hemi: '0' });
  assert.strictEqual(south.properties.time.timestamp.value, north.properties.time.timestamp.value);
  assert.strictEqual(south.properties.kind.longText.text, 'highest');
  assert.strictEqual(north.properties.kind.longText.text, 'lowest');
});

test('texts follow the language of the phone', function () {
  var p = read('now', NOW, SOUTH, 'de-DE').data.instances[0].properties;
  assert.strictEqual(p.ascending.shortText.text, 'Aufsteigend');
  assert.strictEqual(p.next_phase.shortText.text, 'Neumond');
});

test('an unknown item is an error', function () {
  assert.strictEqual(read('later', NOW, SOUTH).error, 'INVALID_REQUEST');
});

if (failures) {
  console.log(failures + ' failed');
  process.exit(1);
}
