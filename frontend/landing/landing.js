/* 
   VoteStack Landing — Premium Scroll Effects
    */

document.addEventListener('DOMContentLoaded', () => {

  if (typeof lucide !== 'undefined') lucide.createIcons();

  initViewportFix();
  initHeader();
  initMobileMenu();
  initScrollProgress();
  initAOS();
  initStagger();
  initSpotlightSlide();
  initStepReveal();
  initSectionLabels();
  initSectionTitleLines();
  initVideoReveal();
  initComplianceBadges();
  initFeatCards3D();
  initParallax();
  initCounters();
  initFAQ();
  initVideo();
  initAuthState();

});

/*
   VIEWPORT FIX — iOS Safari reports wrong 100vh on initial load
   because the address bar is still visible. We set --vh as a CSS
   variable matching the real inner height, and update it on resize.
 */
function initViewportFix() {
  const setVh = () => {
    const vh = window.innerHeight * 0.01;
    document.documentElement.style.setProperty('--vh', `${vh}px`);
  };

  // Run immediately on DOMContentLoaded
  setVh();

  // Run again after full page load (fonts, images, video metadata)
  window.addEventListener('load', setVh, { passive: true });

  // Run on resize (orientation change, window resize)
  window.addEventListener('resize', setVh, { passive: true });

  // orientationchange fires before the new dimensions settle — delay slightly
  window.addEventListener('orientationchange', () => {
    setTimeout(setVh, 300);
  }, { passive: true });
}

/* 
   HEADER — scroll shadow + colour swap
 */
function initHeader() {
  const header = document.getElementById('header');
  if (!header) return;

  const onScroll = () => header.classList.toggle('scrolled', window.scrollY > 10);
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const href = a.getAttribute('href');
      if (!href || href.length < 2) return;
      const target = document.querySelector(href);
      if (!target) return;
      e.preventDefault();
      const offset = header.offsetHeight + 12;
      window.scrollTo({ top: target.offsetTop - offset, behavior: 'smooth' });
      closeMobileMenu();
    });
  });
}

/* 
   MOBILE MENU
 */
function initMobileMenu() {
  const burger = document.getElementById('burger');
  const drawer = document.getElementById('mobileDrawer');
  if (!burger || !drawer) return;

  burger.addEventListener('click', () => {
    const open = drawer.classList.toggle('open');
    burger.classList.toggle('is-open', open);
    burger.setAttribute('aria-expanded', open);
    drawer.setAttribute('aria-hidden', !open);
  });

  // Close on outside click
  document.addEventListener('click', (e) => {
    if (!burger.contains(e.target) && !drawer.contains(e.target)) {
      closeMobileMenu();
    }
  });
}

function closeMobileMenu() {
  const drawer = document.getElementById('mobileDrawer');
  const burger = document.getElementById('burger');
  if (!drawer || !burger) return;
  drawer.classList.remove('open');
  burger.classList.remove('is-open');
  burger.setAttribute('aria-expanded', 'false');
  drawer.setAttribute('aria-hidden', 'true');
}

/* 
   SCROLL PROGRESS BAR
 */
function initScrollProgress() {
  const bar = document.createElement('div');
  bar.className = 'scroll-progress';
  document.body.prepend(bar);

  const update = () => {
    const scrolled = window.scrollY;
    const total    = document.documentElement.scrollHeight - window.innerHeight;
    bar.style.width = total > 0 ? `${(scrolled / total) * 100}%` : '0%';
  };
  window.addEventListener('scroll', update, { passive: true });
  update();
}

/* 
   AOS — base fade-up + variant support
 */
function initAOS() {
  const els = document.querySelectorAll('[data-aos]');
  if (!els.length) return;

  els.forEach(el => {
    const delay = el.getAttribute('data-aos-delay');
    if (delay) el.style.transitionDelay = `${delay}ms`;
  });

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('aos-in');
        io.unobserve(entry.target);
      }
    });
  }, { threshold: 0.1, rootMargin: '0px 0px -50px 0px' });

  els.forEach(el => io.observe(el));
}

/* 
   STAGGER — [data-stagger] containers
   Children animate in with cascading delays
 */
function initStagger() {
  const groups = document.querySelectorAll('[data-stagger]');
  if (!groups.length) return;

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('stagger-in');
        io.unobserve(entry.target);
      }
    });
  }, { threshold: 0.1, rootMargin: '0px 0px -60px 0px' });

  groups.forEach(g => io.observe(g));
}

/* 
   SPOTLIGHT SLIDE — content in from left, image from right
   (and reverse for the alternating spotlight)
 */
function initSpotlightSlide() {
  const spotlights = document.querySelectorAll('.spotlight');
  if (!spotlights.length) return;

  spotlights.forEach((spotlight, i) => {
    const isReverse = spotlight.classList.contains('spotlight--reverse');
    const content   = spotlight.querySelector('.spotlight-content');
    const media     = spotlight.querySelector('.spotlight-media');

    if (content) content.setAttribute('data-slide', isReverse ? 'right' : 'left');
    if (media)   media.setAttribute('data-slide',   isReverse ? 'left'  : 'right');

    const io = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          // Stagger: content first, media 150ms later
          if (content) {
            setTimeout(() => content.classList.add('slide-in'), 0);
          }
          if (media) {
            setTimeout(() => media.classList.add('slide-in'), 160);
          }
          io.unobserve(entry.target);
        }
      });
    }, { threshold: 0.15, rootMargin: '0px 0px -60px 0px' });

    io.observe(spotlight);
  });
}

/* 
   STEP REVEAL — left border line draws down
 */
function initStepReveal() {
  const steps = document.querySelectorAll('.step');
  if (!steps.length) return;

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('step-revealed');
        io.unobserve(entry.target);
      }
    });
  }, { threshold: 0.2 });

  steps.forEach(s => io.observe(s));
}

/* 
   SECTION LABELS — wipe reveal effect
 */
function initSectionLabels() {
  const labels = document.querySelectorAll('.section-label');
  if (!labels.length) return;

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('label-revealed');
        io.unobserve(entry.target);
      }
    });
  }, { threshold: 0.5 });

  labels.forEach(l => io.observe(l));
}

/* 
   SECTION TITLE LINES — underline expands on scroll
 */
function initSectionTitleLines() {
  document.querySelectorAll('.section-title').forEach(title => {
    // Insert a line element after the title
    const line = document.createElement('span');
    line.className = 'section-title-line';
    title.after(line);

    const io = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) {
          setTimeout(() => line.classList.add('line-expanded'), 300);
          io.unobserve(entry.target);
        }
      });
    }, { threshold: 0.4 });

    io.observe(title);
  });
}

/* 
   VIDEO FRAME — scale + fade reveal
 */
function initVideoReveal() {
  const frame = document.querySelector('.video-frame');
  if (!frame) return;

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        frame.classList.add('video-in');
        io.unobserve(frame);
      }
    });
  }, { threshold: 0.15 });

  io.observe(frame);
}

/* 
   COMPLIANCE BADGES — staggered pop in
 */
function initComplianceBadges() {
  const row = document.querySelector('.compliance-row');
  if (!row) return;

  const badges = row.querySelectorAll('.compliance-badge');

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        badges.forEach((badge, i) => {
          setTimeout(() => badge.classList.add('badge-in'), i * 80);
        });
        io.unobserve(entry.target);
      }
    });
  }, { threshold: 0.3 });

  io.observe(row);
}

/* 
   FEAT CARDS — 3D tilt on mouse move (desktop only)
 */
function initFeatCards3D() {
  if (window.matchMedia('(hover: none)').matches) return; // skip touch

  document.querySelectorAll('.feat-card').forEach(card => {
    card.addEventListener('mousemove', e => {
      const rect = card.getBoundingClientRect();
      const x = e.clientX - rect.left; // 0 → width
      const y = e.clientY - rect.top;  // 0 → height
      const cx = rect.width  / 2;
      const cy = rect.height / 2;
      const rotX = ((y - cy) / cy) * -6; // max ±6deg
      const rotY = ((x - cx) / cx) *  6;
      card.style.transform =
        `perspective(900px) rotateX(${rotX}deg) rotateY(${rotY}deg) translateY(-4px)`;
      card.style.boxShadow = '0 16px 48px rgba(0,0,0,.14)';
    });

    card.addEventListener('mouseleave', () => {
      card.style.transform = '';
      card.style.boxShadow = '';
    });
  });
}

/* 
   PARALLAX — [data-parallax] elements
 */
function initParallax() {
  const els = document.querySelectorAll('[data-parallax]');
  if (!els.length) return;

  let ticking = false;

  const update = () => {
    const scrollY = window.pageYOffset;
    els.forEach(el => {
      const rect  = el.getBoundingClientRect();
      const speed = parseFloat(el.getAttribute('data-speed')) || 0.1;
      if (rect.top < window.innerHeight && rect.bottom > 0) {
        const elTop  = rect.top + scrollY;
        const offset = (scrollY - elTop + window.innerHeight) * speed;
        el.style.transform = `translateY(${offset}px)`;
      }
    });
    ticking = false;
  };

  window.addEventListener('scroll', () => {
    if (!ticking) { window.requestAnimationFrame(update); ticking = true; }
  }, { passive: true });
  update();
}

/* 
   COUNTERS — animate stat numbers when they scroll into view
 */
function initCounters() {
  // Map display text → target number + suffix
  const STATS = [
    { selector: '.stat-item:nth-child(1) .stat-num', end: 99.9, decimals: 1, suffix: '%' },
    { selector: '.stat-item:nth-child(3) .stat-num', end: 50,   decimals: 0, prefix: '<', suffix: 'ms' },
    { selector: '.stat-item:nth-child(5) .stat-num', end: 10,   decimals: 0, suffix: 'k+' },
    { selector: '.stat-item:nth-child(7) .stat-num', end: 256,  decimals: 0, suffix: '-bit' },
  ];

  STATS.forEach(cfg => {
    const el = document.querySelector(cfg.selector);
    if (!el) return;

    const io = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (!entry.isIntersecting) return;
        animateCounter(el, cfg);
        entry.target.closest('.stat-item').setAttribute('data-counted', '');
        io.unobserve(entry.target);
      });
    }, { threshold: 0.5 });

    io.observe(el);
  });
}

function animateCounter(el, { end, decimals = 0, prefix = '', suffix = '' }) {
  const duration = 1600;
  const start    = performance.now();

  const easeOutQuart = t => 1 - Math.pow(1 - t, 4);

  const tick = (now) => {
    const progress = Math.min((now - start) / duration, 1);
    const value    = end * easeOutQuart(progress);
    el.innerHTML   = `${prefix}${value.toFixed(decimals)}<small>${suffix}</small>`;
    if (progress < 1) requestAnimationFrame(tick);
    else {
      el.innerHTML = `${prefix}${end.toFixed(decimals)}<small>${suffix}</small>`;
      el.closest('.stat-item').classList.add('count-done');
    }
  };

  requestAnimationFrame(tick);
}

/* 
   FAQ — native <details>, refresh icons
 */
function initFAQ() {
  document.querySelectorAll('.faq-item').forEach(item => {
    item.addEventListener('toggle', () => {
      if (typeof lucide !== 'undefined') lucide.createIcons();
    });
  });
}

/* 
   VIDEO — play/pause toggle
 */
function initVideo() {
  const video   = document.querySelector('.video-frame video');
  const overlay = document.getElementById('videoOverlay');
  const playBtn = document.getElementById('playBtn');
  if (!video || !overlay || !playBtn) return;

  const updateIcon = () => {
    const icon = video.paused ? 'play' : 'pause';
    playBtn.innerHTML = `<i data-lucide="${icon}"></i>`;
    if (typeof lucide !== 'undefined') lucide.createIcons();
    overlay.classList.toggle('visible', video.paused);
  };

  video.addEventListener('canplay', () => {
    updateIcon();
    if (!video.paused) {
      setTimeout(() => overlay.classList.remove('visible'), 1200);
    }
  });

  playBtn.addEventListener('click', () => {
    if (video.paused) { video.play();  overlay.classList.remove('visible'); }
    else              { video.pause(); overlay.classList.add('visible'); }
    updateIcon();
  });

  video.addEventListener('pause', updateIcon);
  video.addEventListener('play',  updateIcon);
}

/* 
   AUTH STATE — swap CTA if already logged in
 */
function initAuthState() {
  if (typeof Auth === 'undefined' || !Auth.isLoggedIn()) return;
  const user  = Auth.user();
  const ctaEl = document.getElementById('headerCta');
  if (ctaEl && user) {
    ctaEl.innerHTML = `
      <span style="font-size:14px;color:#6e6e6e;font-weight:500">
        Hi, ${user.name || 'there'}
      </span>
      <a href="../dashboard/index.html" class="btn-filled">
        Dashboard
        <svg xmlns="http://www.w3.org/2000/svg" width="17" height="17" viewBox="0 0 24 24"
          fill="none" stroke="currentColor" stroke-width="2"
          stroke-linecap="round" stroke-linejoin="round">
          <line x1="5" y1="12" x2="19" y2="12"/>
          <polyline points="12 5 19 12 12 19"/>
        </svg>
      </a>`;
  }
}
