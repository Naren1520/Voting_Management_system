

document.addEventListener('DOMContentLoaded', () => {
  if (!Auth.requireAuth()) return;
  populateProfile();
});

function populateProfile() {
  const user = Auth.user();
  if (!user) return;

  const name  = user.name  || 'Unknown';
  const email = user.email || 'Unknown';

  // Avatar initials (up to 2 chars)
  const initials = name
    .split(' ')
    .map(w => w[0])
    .join('')
    .toUpperCase()
    .slice(0, 2);

  setEl('profHeroAvatar', initials);
  setEl('profHeroName',   name);
  setEl('profNameVal',    name);
  setEl('profEmailVal',   email);
}

document.getElementById('newPassword').addEventListener('input', function () {
  const val = this.value;
  const strengthEl = document.getElementById('pwdStrength');
  const fillEl     = document.getElementById('pwdStrengthFill');
  const labelEl    = document.getElementById('pwdStrengthLabel');

  if (!val) {
    strengthEl.style.display = 'none';
    return;
  }

  strengthEl.style.display = 'flex';

  let score = 0;
  if (val.length >= 6)  score++;
  if (val.length >= 10) score++;
  if (/[A-Z]/.test(val)) score++;
  if (/[0-9]/.test(val)) score++;
  if (/[^A-Za-z0-9]/.test(val)) score++;

  const levels = [
    { pct: '20%', color: '#dc2626', label: 'Weak' },
    { pct: '40%', color: '#ea580c', label: 'Fair' },
    { pct: '60%', color: '#d97706', label: 'Okay' },
    { pct: '80%', color: '#16a34a', label: 'Good' },
    { pct: '100%',color: '#15803d', label: 'Strong' },
  ];
  const lvl = levels[Math.min(score - 1, 4)] || levels[0];

  fillEl.style.width      = lvl.pct;
  fillEl.style.background = lvl.color;
  labelEl.textContent     = lvl.label;
  labelEl.style.color     = lvl.color;
});

function togglePwd(inputId, btn) {
  const input  = document.getElementById(inputId);
  const isText = input.type === 'text';
  input.type   = isText ? 'password' : 'text';

  btn.querySelector('.eye-show').style.display = isText ? 'block' : 'none';
  btn.querySelector('.eye-hide').style.display = isText ? 'none'  : 'block';
}


async function submitPasswordChange() {
  const currentPwd  = document.getElementById('currentPassword').value.trim();
  const newPwd      = document.getElementById('newPassword').value.trim();
  const confirmPwd  = document.getElementById('confirmPassword').value.trim();
  const btn         = document.getElementById('updatePwdBtn');

  // Clear previous states
  clearInputErrors();
  hideMsg('pwdMsg');

  // --- Validation ---
  if (!currentPwd) {
    setInputError('currentPassword');
    showMsg('pwdMsg', 'Please enter your current password.', 'error');
    return;
  }
  if (!newPwd) {
    setInputError('newPassword');
    showMsg('pwdMsg', 'Please enter a new password.', 'error');
    return;
  }
  if (newPwd.length < 6) {
    setInputError('newPassword');
    showMsg('pwdMsg', 'New password must be at least 6 characters.', 'error');
    return;
  }
  if (newPwd === currentPwd) {
    setInputError('newPassword');
    showMsg('pwdMsg', 'New password must be different from current password.', 'error');
    return;
  }
  if (newPwd !== confirmPwd) {
    setInputError('confirmPassword');
    showMsg('pwdMsg', 'Passwords do not match.', 'error');
    return;
  }

  // Submit 
  btn.disabled = true;
  btn.innerHTML = '<span class="pwd-spinner"></span> Updating…';

  const res = await API.changePassword(currentPwd, newPwd);

  btn.disabled = false;
  btn.innerHTML = `
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
      <path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>
    </svg>
    Update Password`;

  if (res.success) {
    // Clear fields
    document.getElementById('currentPassword').value = '';
    document.getElementById('newPassword').value     = '';
    document.getElementById('confirmPassword').value = '';
    document.getElementById('pwdStrength').style.display = 'none';

    showMsg('pwdMsg', 'Password updated successfully!', 'success');
    showToast('Password updated successfully!', 'success');
  } else {
    // Detect wrong current password from common server responses
    const msg = res.message || '';
    const isWrongPassword =
      msg.toLowerCase().includes('wrong') ||
      msg.toLowerCase().includes('incorrect') ||
      msg.toLowerCase().includes('invalid') ||
      msg.toLowerCase().includes('current') ||
      msg.toLowerCase().includes('mismatch') ||
      res.status === 401;

    if (isWrongPassword) {
      setInputError('currentPassword');
      showMsg('pwdMsg', 'Wrong password. Please try again.', 'error');
      showToast('Wrong password.', 'error');
    } else {
      showMsg('pwdMsg', msg || 'Failed to update password. Please try again.', 'error');
      showToast('Failed to update password.', 'error');
    }
  }
}


function setInputError(id) {
  const el = document.getElementById(id);
  if (el) el.classList.add('error');
}
function clearInputErrors() {
  ['currentPassword', 'newPassword', 'confirmPassword'].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.classList.remove('error');
  });
}

function showMsg(id, text, type) {
  const el = document.getElementById(id);
  if (!el) return;
  el.textContent = text;
  el.className   = 'prof-msg ' + type;
}
function hideMsg(id) {
  const el = document.getElementById(id);
  if (el) el.className = 'prof-msg';
}


let toastTimer = null;
function showToast(msg, type = 'success') {
  const t = document.getElementById('profToast');
  if (!t) return;
  t.querySelector('.toast-msg').textContent = msg;
  t.className = 'prof-toast show ' + (type === 'error' ? 'error-toast' : 'success-toast');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => t.classList.remove('show'), 3500);
}


function setEl(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}
