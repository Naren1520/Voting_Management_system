/**
 * VoteStack User Guide — guide.js
 * Handles: guide toggle, sidebar nav generation,
 * active section tracking, reveal animations,
 * reading progress, back-to-top.
 */

/* ─────────────────────────────────────────────
   State
───────────────────────────────────────────── */
let currentGuide = 'voter'; // 'voter' | 'admin'

/* ─────────────────────────────────────────────
   Sidebar config — mirrors section IDs in HTML
───────────────────────────────────────────── */
const NAV_CONFIG = {
  voter: [
    {
      label: 'Voter Guide',
      links: [
        { id: 'v-getting-started', text: 'Getting Started',   icon: '<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>' },
        { id: 'v-casting-vote',    text: 'Casting Your Vote', icon: '<svg viewBox="0 0 24 24"><path d="M9 11l3 3L22 4"/><path d="M21 12v7a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11"/></svg>' },
        { id: 'v-faq',             text: 'FAQ',               icon: '<svg viewBox="0 0 24 24"><path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/></svg>' },
      ]
    }
  ],
  admin: [
    {
      label: 'Admin Guide',
      links: [
        { id: 'a-account-setup',    text: 'Account Setup',       icon: '<svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 20c0-4 3.6-7 8-7s8 3 8 7"/></svg>' },
        { id: 'a-create-election',  text: 'Creating Elections',   icon: '<svg viewBox="0 0 24 24"><rect x="3" y="3" width="18" height="18" rx="3"/><path d="M9 12h6M12 9v6"/></svg>' },
        { id: 'a-manage-election',  text: 'Managing Elections',   icon: '<svg viewBox="0 0 24 24"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4z"/></svg>' },
        { id: 'a-face-verify',      text: 'Face Verification',    icon: '<svg viewBox="0 0 24 24"><circle cx="12" cy="8" r="4"/><path d="M4 20c0-4 3.6-7 8-7s8 3 8 7"/><polyline points="16 11 18 13 22 9"/></svg>' },
        { id: 'a-faq',              text: 'FAQ',                   icon: '<svg viewBox="0 0 24 24"><path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/></svg>' },
      ]
    }
  ]
};

/* ─────────────────────────────────────────────
   Init
───────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', () => {
  // Restore saved guide from localStorage
  const saved = localStorage.getItem('vs_guide') || 'voter';
  // Apply immediately without animation on first load
  applyGuide(saved, false);

  buildSidebar(saved);
  initToggleSlider();
  initReveal();
  initActiveSection();
  initProgress();
  initBackToTop();
});

/* ─────────────────────────────────────────────
   Guide toggle
───────────────────────────────────────────── */
function switchGuide(guide) {
  if (guide === currentGuide) return;
  applyGuide(guide, true);
  localStorage.setItem('vs_guide', guide);
  buildSidebar(guide);
  initToggleSlider();
  // Re-init observers for newly visible sections
  initReveal();
  initActiveSection();
  // Scroll to top of main content smoothly
  const main = document.getElementById('guideMain');
  if (main) main.scrollIntoView({ behavior: 'smooth', block: 'start' });
}

function applyGuide(guide, animate) {
  currentGuide = guide;

  const voterPanel = document.getElementById('voterGuide');
  const adminPanel = document.getElementById('adminGuide');
  const btnVoter   = document.getElementById('toggleVoter');
  const btnAdmin   = document.getElementById('toggleAdmin');

  // Toggle content panels
  if (guide === 'voter') {
    show(voterPanel, animate);
    hide(adminPanel);
    btnVoter.classList.add('active');
    btnAdmin.classList.remove('active');
    btnVoter.setAttribute('aria-selected', 'true');
    btnAdmin.setAttribute('aria-selected', 'false');
  } else {
    show(adminPanel, animate);
    hide(voterPanel);
    btnAdmin.classList.add('active');
    btnVoter.classList.remove('active');
    btnAdmin.setAttribute('aria-selected', 'true');
    btnVoter.setAttribute('aria-selected', 'false');
  }
}

function show(el, animate) {
  if (!el) return;
  el.classList.add('active');
  if (animate) {
    el.style.opacity = '0';
    el.style.transform = 'translateY(10px)';
    el.style.transition = 'opacity .3s ease, transform .3s ease';
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        el.style.opacity = '1';
        el.style.transform = 'translateY(0)';
      });
    });
    setTimeout(() => {
      el.style.transition = '';
      el.style.opacity = '';
      el.style.transform = '';
    }, 350);
  }
}

function hide(el) {
  if (!el) return;
  el.classList.remove('active');
  el.style.opacity = '';
  el.style.transform = '';
  el.style.transition = '';
}

/* ─────────────────────────────────────────────
   Toggle slider — pill slides under active btn
───────────────────────────────────────────── */
function initToggleSlider() {
  const toggle    = document.getElementById('guideToggle');
  const slider    = document.getElementById('toggleSlider');
  if (!toggle || !slider) return;

  const activeBtn = toggle.querySelector('.toggle-btn.active');
  if (!activeBtn) return;

  // Use offsetLeft/offsetWidth for reliable sub-pixel positioning
  // regardless of inline-flex vs flex (full-width mobile) mode
  const offsetLeft = activeBtn.offsetLeft;
  const btnWidth   = activeBtn.offsetWidth;
  const btnHeight  = activeBtn.offsetHeight;

  slider.style.width  = btnWidth  + 'px';
  slider.style.height = btnHeight + 'px';
  // Subtract the 5px padding on the toggle container
  slider.style.transform = `translateX(${offsetLeft - 5}px)`;
}

// Update slider on window resize
window.addEventListener('resize', initToggleSlider, { passive: true });
// Re-init scroll spy on resize — header height changes between breakpoints
window.addEventListener('resize', () => {
  clearTimeout(window._resizeTimer);
  window._resizeTimer = setTimeout(() => {
    initActiveSection();
  }, 200);
}, { passive: true });

/* ─────────────────────────────────────────────
   Sidebar generation
───────────────────────────────────────────── */
function buildSidebar(guide) {
  const nav = document.getElementById('sidebarNav');
  if (!nav) return;

  const config = NAV_CONFIG[guide] || [];
  nav.innerHTML = '';

  config.forEach(group => {
    const label = document.createElement('div');
    label.className = 'sidebar-section-label';
    label.textContent = group.label;
    nav.appendChild(label);

    group.links.forEach(link => {
      const a = document.createElement('a');
      a.className  = 'sidebar-link';
      a.href       = '#' + link.id;
      a.dataset.id = link.id;
      a.innerHTML  = link.icon + `<span>${link.text}</span>`;
      a.addEventListener('click', e => {
        e.preventDefault();
        const target = document.getElementById(link.id);
        if (target) {
          // Read computed --header-h so offset is correct at every breakpoint
          const rawH   = getComputedStyle(document.documentElement).getPropertyValue('--header-h').trim();
          const offset = (parseInt(rawH) || 64) + 16;
          const y = target.getBoundingClientRect().top + window.scrollY - offset;
          window.scrollTo({ top: y, behavior: 'smooth' });
        }
      });
      nav.appendChild(a);
    });
  });
}

/* ─────────────────────────────────────────────
   Active section tracking (scroll spy)
───────────────────────────────────────────── */
let sectionObserver = null;

function initActiveSection() {
  if (sectionObserver) sectionObserver.disconnect();

  const sections = document.querySelectorAll(
    `#${currentGuide === 'voter' ? 'voterGuide' : 'adminGuide'} .guide-section`
  );

  // Read the actual computed --header-h (changes per breakpoint)
  const rawH = getComputedStyle(document.documentElement)
    .getPropertyValue('--header-h').trim();
  const headerH = parseInt(rawH) || 64;

  sectionObserver = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (!entry.isIntersecting) return;
      const id    = entry.target.id;
      const links = document.querySelectorAll('.sidebar-link');
      links.forEach(l => {
        l.classList.toggle('is-active', l.dataset.id === id);
      });
      // Scroll active link into view in mobile horizontal nav
      const active = document.querySelector('.sidebar-link.is-active');
      if (active) active.scrollIntoView({ inline: 'nearest', block: 'nearest' });
    });
  }, {
    rootMargin: `-${headerH + 16}px 0px -55% 0px`,
    threshold: 0
  });

  sections.forEach(s => sectionObserver.observe(s));
}

/* ─────────────────────────────────────────────
   Reveal animations (Intersection Observer)
───────────────────────────────────────────── */
let revealObserver = null;

function initReveal() {
  if (revealObserver) revealObserver.disconnect();

  const els = document.querySelectorAll('.reveal:not(.in-view)');

  revealObserver = new IntersectionObserver((entries) => {
    entries.forEach((entry, i) => {
      if (!entry.isIntersecting) return;
      // Stagger siblings in the same parent
      const siblings = Array.from(entry.target.parentElement.querySelectorAll('.reveal'));
      const idx = siblings.indexOf(entry.target);
      setTimeout(() => {
        entry.target.classList.add('in-view');
      }, idx * 70);
      revealObserver.unobserve(entry.target);
    });
  }, { threshold: 0.08, rootMargin: '0px 0px -40px 0px' });

  els.forEach(el => revealObserver.observe(el));
}

/* ─────────────────────────────────────────────
   Reading progress bar
───────────────────────────────────────────── */
function initProgress() {
  const bar = document.getElementById('progressBar');
  if (!bar) return;

  window.addEventListener('scroll', () => {
    const scrolled = window.scrollY;
    const total    = document.documentElement.scrollHeight - window.innerHeight;
    bar.style.width = total > 0 ? `${(scrolled / total) * 100}%` : '0%';
  }, { passive: true });
}

/* ─────────────────────────────────────────────
   Back to top
───────────────────────────────────────────── */
function initBackToTop() {
  const btn = document.getElementById('backToTop');
  if (!btn) return;

  window.addEventListener('scroll', () => {
    btn.classList.toggle('visible', window.scrollY > 400);
  }, { passive: true });

  btn.addEventListener('click', () => {
    window.scrollTo({ top: 0, behavior: 'smooth' });
  });
}
