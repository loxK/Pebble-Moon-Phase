/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Minimal Pebble timeline helper (personal pins).
 *
 * Pushes/deletes pins for the current user via the public timeline web API,
 * authenticating with the per-user timeline token from Pebble.getTimelineToken.
 * No backend server is required.
 *
 * IMPORTANT -- the timeline host is NOT defined by the watch firmware; it is
 * whatever timeline service the connected phone app issues tokens for. The
 * value below matches the repebble developer docs; if your watch is paired to
 * a different ecosystem (e.g. Rebble), change API_URL_ROOT accordingly.
 */

var API_URL_ROOT = 'https://timeline-api.getpebble.com/';

/** Send a pin request (PUT or DELETE) to the user's timeline. */
function timelineRequest(pin, type, callback) {
  var url = API_URL_ROOT + 'v1/user/pins/' + pin.id;

  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    if (callback) callback(this.status, this.responseText);
  };
  xhr.onerror = function () {
    if (callback) callback(0, 'network error');
  };
  xhr.ontimeout = function () {
    if (callback) callback(0, 'timeout');
  };
  xhr.open(type, url);
  xhr.timeout = 20000;  // ensure the callback always fires, even if the net hangs

  Pebble.getTimelineToken(function (token) {
    xhr.setRequestHeader('Content-Type', 'application/json');
    xhr.setRequestHeader('X-User-Token', '' + token);
    xhr.send(JSON.stringify(pin));
  }, function (error) {
    console.log('timeline: token error: ' + error);
    if (callback) callback(0, 'no token');
  });
}

function insertUserPin(pin, callback) {
  timelineRequest(pin, 'PUT', callback);
}

function deleteUserPin(pin, callback) {
  timelineRequest(pin, 'DELETE', callback);
}

module.exports = {
  insertUserPin: insertUserPin,
  deleteUserPin: deleteUserPin,
  API_URL_ROOT: API_URL_ROOT,
};
