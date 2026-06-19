/* ESP32 Base Template — app.js
 * Shared JS helpers used by all pages.
 */

'use strict';

/**
 * POST JSON to a REST endpoint and return the parsed response.
 * @param {string} url
 * @param {object} data
 * @returns {Promise<object>}
 */
async function postJSON(url, data) {
  const resp = await fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data),
  });
  return resp.json();
}

/**
 * Show a status message in the element with id="status-msg".
 * @param {string} msg
 * @param {'ok'|'error'} type
 */
function showStatus(msg, type) {
  const el = document.getElementById('status-msg');
  if (!el) return;
  el.textContent = msg;
  el.className = type || '';
}

/** Mark the current page's nav link as active. */
function markActiveNav() {
  const path = window.location.pathname.replace(/\/$/, '') || '/';
  document.querySelectorAll('nav a').forEach(a => {
    const href = a.getAttribute('href').replace(/\/$/, '') || '/';
    a.classList.toggle('active', href === path);
  });
}

document.addEventListener('DOMContentLoaded', markActiveNav);
