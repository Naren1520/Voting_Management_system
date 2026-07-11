/* 
   VoteStack Auth - Shared JS
   login.html + signup.html
    */

document.addEventListener('DOMContentLoaded', () => {
  if (typeof Auth !== 'undefined') Auth.requireGuest();
  initEyeToggles();
  initInputAnimations();
  if (typeof initStrength === 'function') initStrength();

  // Dismiss skeleton - DOM is ready, layout is painted
  hideAuthSkeleton();

  // Ping the server as soon as the page loads so Render's free tier
  // starts waking up before the user clicks the submit button.
  // The result is ignored - this is purely a warm-up call.
  fetch((window.API_BASE || 'http://localhost:8080') + '/health', {
    method: 'GET',
    signal: AbortSignal.timeout ? AbortSignal.timeout(35000) : undefined
  }).catch(() => {});
});

function hideAuthSkeleton() {
  const sk = document.getElementById('authSkeletonOverlay');
  if (!sk) return;
  sk.classList.add('ask-hidden');
  sk.addEventListener('transitionend', () => sk.remove(), { once: true });
}

/* 
   Password visibility toggle
 */
function initEyeToggles() {
  document.querySelectorAll('.eye-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const wrap  = btn.closest('.password-wrap');
      const input = wrap.querySelector('.input-field');
      const isHidden = input.type === 'password';
      input.type = isHidden ? 'text' : 'password';
      btn.setAttribute('aria-label', isHidden ? 'Hide password' : 'Show password');
      // Swap icon
      btn.innerHTML = isHidden ? iconEyeOff() : iconEye();
    });
  });
}

/* 
   Subtle label float + border colour on focus
 */
function initInputAnimations() {
  document.querySelectorAll('.input-field').forEach(input => {
    const group = input.closest('.input-group');
    if (!group) return;
    const label = group.querySelector('label');
    if (!label) return;

    const activate   = () => label.classList.add('active');
    const deactivate = () => { if (!input.value) label.classList.remove('active'); };

    if (input.value) label.classList.add('active');
    input.addEventListener('focus', activate);
    input.addEventListener('blur',  deactivate);
  });
}

/* 
   Status message
 */
function showMsg(text, type, id = 'authMsg') {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = text;
  el.className   = 'auth-msg ' + type;
  el.style.display = 'block';
  // scroll into view on mobile
  el.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function hideMsg(id = 'authMsg') {
  const el = document.getElementById(id);
  if (el) el.style.display = 'none';
}

/* 
   Button loading state
 */
function setLoading(btn, loading, defaultLabel) {
  if (loading) {
    btn.disabled = true;
    btn.innerHTML = `<span class="btn-spinner"></span> ${defaultLabel}…`;
  } else {
    btn.disabled  = false;
    btn.innerHTML = defaultLabel + arrowIcon();
  }
}

/* 
   LOGIN handler  (called from login.html)
 */
async function handleLogin(e) {
  e.preventDefault();
  hideMsg();

  const email    = document.getElementById('email').value.trim();
  const password = document.getElementById('password').value;
  const btn      = document.getElementById('submitBtn');

  if (!email || !password) {
    showMsg('Please enter your email and password.', 'error'); return;
  }
  if (!/\S+@\S+\.\S+/.test(email)) {
    showMsg('Please enter a valid email address.', 'error'); return;
  }

  setLoading(btn, true, 'Signing in');

  const res = await API.login(email, password);

  if (res.success) {
    Auth.save(res.token, res.user);
    showMsg('Welcome back! Redirecting…', 'success');
    setTimeout(() => window.location.href = '/dashboard/index.html', 900);
  } else {
    showMsg(res.message || 'Login failed. Please try again.', 'error');
    setLoading(btn, false, 'Sign in');
  }
}

/* 
   SIGNUP handler  (called from signup.html)
 */
async function handleSignup(e) {
  e.preventDefault();
  hideMsg();

  const name     = document.getElementById('name').value.trim();
  const email    = document.getElementById('email').value.trim();
  const password = document.getElementById('password').value;
  const btn      = document.getElementById('submitBtn');

  if (!name || !email || !password) {
    showMsg('Please fill in all fields.', 'error'); return;
  }
  if (!/\S+@\S+\.\S+/.test(email)) {
    showMsg('Please enter a valid email address.', 'error'); return;
  }
  if (password.length < 6) {
    showMsg('Password must be at least 6 characters.', 'error'); return;
  }

  setLoading(btn, true, 'Creating account');

  const res = await API.signup(name, email, password);

  if (res.success) {
    Auth.save(res.token, res.user);
    showMsg('Account created! Redirecting to your dashboard…', 'success');
    setTimeout(() => window.location.href = '/dashboard/index.html', 1000);
  } else {
    showMsg(res.message || 'Signup failed. Please try again.', 'error');
    setLoading(btn, false, 'Create account');
  }
}

/* 
   Password strength meter  (signup only)
 */
function initStrength() {
  const input = document.getElementById('password');
  if (!input) return;

  input.addEventListener('input', () => {
    const v    = input.value;
    const fill = document.getElementById('strengthFill');
    const lbl  = document.getElementById('strengthLabel');
    if (!fill || !lbl) return;

    if (!v) {
      fill.className = 'strength-fill';
      lbl.className  = 'strength-label';
      lbl.textContent = '';
      return;
    }

    const score = getScore(v);
    const map   = ['', 'weak', 'medium', 'strong'];
    const label = ['', 'Weak', 'Fair', 'Strong'];

    fill.className  = 'strength-fill ' + (map[score]  || '');
    lbl.className   = 'strength-label ' + (map[score] || '');
    lbl.textContent = label[score] || '';
  });
}

function getScore(v) {
  let s = 0;
  if (v.length >= 6)  s++;
  if (v.length >= 10) s++;
  if (/[A-Z]/.test(v) && /[0-9!@#$%^&*]/.test(v)) s++;
  return Math.min(s, 3);
}

/* 
   Inline SVG icons (avoids external dependency on auth pages)
 */
function iconEye() {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="17" height="17" viewBox="0 0 24 24"
    fill="none" stroke="currentColor" stroke-width="2"
    stroke-linecap="round" stroke-linejoin="round">
    <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"/>
    <circle cx="12" cy="12" r="3"/>
  </svg>`;
}

function iconEyeOff() {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="17" height="17" viewBox="0 0 24 24"
    fill="none" stroke="currentColor" stroke-width="2"
    stroke-linecap="round" stroke-linejoin="round">
    <path d="M17.94 17.94A10.07 10.07 0 0 1 12 20c-7 0-11-8-11-8a18.45 18.45 0 0 1 5.06-5.94"/>
    <path d="M9.9 4.24A9.12 9.12 0 0 1 12 4c7 0 11 8 11 8a18.5 18.5 0 0 1-2.16 3.19"/>
    <line x1="1" y1="1" x2="23" y2="23"/>
  </svg>`;
}

function arrowIcon() {
  return `<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24"
    fill="none" stroke="currentColor" stroke-width="2.5"
    stroke-linecap="round" stroke-linejoin="round">
    <line x1="5" y1="12" x2="19" y2="12"/>
    <polyline points="12 5 19 12 12 19"/>
  </svg>`;
}
