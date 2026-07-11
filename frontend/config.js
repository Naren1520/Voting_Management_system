// config.js - API base URL
//
// Production (Render):
//   Set VITE_API_BASE or just update the URL below after your
//   first Render deploy. Netlify serves this as a static file
//   so environment variables are not available at runtime -
//   the URL must be baked in at deploy time.
//
// Local development:
//   Change the URL to http://localhost:8080 while developing,
//   then change it back before committing for production.
// 

(function () {
  // Detect localhost automatically so you never need to manually
  var isLocal =
    location.hostname === 'localhost' ||
    location.hostname === '127.0.0.1' ||
    location.hostname === '';

  // Replace this with your actual Render service URL.
  // Find it in: Render dashboard -> your service -> the URL at the top.
  // Example: 'https://voting-system-backend-ab12.onrender.com'
  var PRODUCTION_URL = 'https://votestack-cjom.onrender.com';

  window.API_BASE = isLocal ? 'https://votestack-cjom.onrender.com' : PRODUCTION_URL;

  //http://localhost:8080
})();
