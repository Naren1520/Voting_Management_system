// config.js - API base URL
//
// Production (Render + Netlify):
//   Both isLocal=false branches use PRODUCTION_URL.
//
// Local frontend → Render backend (hybrid dev):
//   Leave isLocal pointing at PRODUCTION_URL (the default below).
//   On Render, set:  SESSION_COOKIE_SAMESITE=None
//   This allows the cross-site cookie to be stored by the browser.
//
// Local frontend → local backend:
//   Change isLocal branch to 'http://localhost:8080'.
//   Run the backend with SESSION_COOKIE_SECURE=0 (run_local.sh does this).

(function () {
  var isLocal =
    location.hostname === 'localhost' ||
    location.hostname === '127.0.0.1' ||
    location.hostname === '';

  // Replace with your actual Render service URL.
  var PRODUCTION_URL = 'https://votestack-cjom.onrender.com';

  // Default: local frontend talks to the Render backend (hybrid dev mode).
  // Switch to 'http://localhost:8080' only when running the backend locally.
  window.API_BASE = isLocal ? PRODUCTION_URL : PRODUCTION_URL;
})();
