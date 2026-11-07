/*
 * Copyright (c) 2026 Laurent Dinclaux <laurent@knc.nc>
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

/**
 * Moon Phase -- localStorage, read and written without throwing.
 */

// Without localStorage nothing is remembered: the hemisphere is fetched and
// every pin pushed again each time.
function lsGet(k) {
  try { return localStorage.getItem(k); } catch (e) { return null; }
}
function lsSet(k, v) {
  try { localStorage.setItem(k, v); } catch (e) { /* no-op */ }
}
function lsRemove(k) {
  try { localStorage.removeItem(k); } catch (e) { /* no-op */ }
}

module.exports = {
  lsGet: lsGet,
  lsSet: lsSet,
  lsRemove: lsRemove,
};
