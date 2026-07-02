/* VoteStack Landing Page Script */

document.addEventListener('DOMContentLoaded', () => {

  // Init icons first so layout is complete before we measure
  if (typeof lucide !== 'undefined') lucide.createIcons();

  initHeader();
  initMobileMenu();
  initAOS();
  initParallax();
  initFAQ();
  initVideo();
  initAuthState();

});

/* ----------------------------------------------------------
   HEADER — scroll shadow
---------------------------------------------------------- */
function initHeader() {
  const header = document.getElementById('header');
  if (!header) return;

  const onScroll = () => header.classList.toggle('scrolled', window.scrollY > 10);
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  // Smooth-scroll anchor links, offset for sticky header
  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const href = a.getAttribute('href');
      if (!href || href.length < 2) return;
      const target = document.querySelector(href);
      if (!target) return;
      e.preventDefault();
      const offset = header.offsetHeight + 12;
      window.scrollTo({ top: target.offsetTop - offset, behavior: 'smooth' });
      // Close mobile drawer if open
      closeMobileMenu();
    });
  });
}

/* ----------------------------------------------------------
   MOBILE MENU
---------------------------------------------------------- */
function initMobileMenu() {
  const burger = document.getElementById('burger');
  const drawer = document.getElementById('mobileDrawer');
  if (!burger || !drawer) return;

  burger.addEventListener('click', () => {
    const open = drawer.classList.toggle('open');
    burger.setAttribute('aria-expanded', open);
    // Animate burger → X
    const spans = burger.querySelectorAll('span');
    if (open) {
      spans[0].style.transform = 'translateY(7px) rotate(45deg)';
      spans[1].style.opacity  = '0';
      spans[2].style.transform = 'translateY(-7px) rotate(-45deg)';
    } else {
      spans.forEach(s => { s.style.transform = ''; s.style.opacity = ''; });
    }
  });
}

function closeMobileMenu() {
  const drawer = document.getElementById('mobileDrawer');
  const burger = document.getElementById('burger');
  if (!drawer || !burger) return;
  drawer.classList.remove('open');
  burger.querySelectorAll('span').forEach(s => { s.style.transform = ''; s.style.opacity = ''; });
}

/* ----------------------------------------------------------
   SCROLL ANIMATIONS (lightweight IntersectionObserver)
---------------------------------------------------------- */
function initAOS() {
  const els = document.querySelectorAll('[data-aos]');
  if (!els.length) return;

  // Apply delay from data-aos-delay attribute
  els.forEach(el => {
    const delay = el.getAttribute('data-aos-delay');
    if (delay) el.style.transitionDelay = `${delay}ms`;
  });

  const io = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        entry.target.classList.add('aos-in');
        io.unobserve(entry.target); // fire once
      }
    });
  }, { threshold: 0.12, rootMargin: '0px 0px -40px 0px' });

  els.forEach(el => io.observe(el));
}

/* ----------------------------------------------------------
   PARALLAX SCROLLING — smooth subtle movement
---------------------------------------------------------- */
function initParallax() {
  const parallaxEls = document.querySelectorAll('[data-parallax]');
  if (!parallaxEls.length) return;

  let ticking = false;

  const updateParallax = () => {
    const scrollY = window.pageYOffset;

    parallaxEls.forEach(el => {
      const rect = el.getBoundingClientRect();
      const elTop = rect.top + scrollY;
      const windowHeight = window.innerHeight;

      // Only apply parallax when element is in viewport
      if (rect.top < windowHeight && rect.bottom > 0) {
        const speed = parseFloat(el.getAttribute('data-speed')) || 0.1;
        const offset = (scrollY - elTop + windowHeight) * speed;
        el.style.transform = `translateY(${offset}px)`;
      }
    });

    ticking = false;
  };

  const onScroll = () => {
    if (!ticking) {
      window.requestAnimationFrame(updateParallax);
      ticking = true;
    }
  };

  window.addEventListener('scroll', onScroll, { passive: true });
  updateParallax(); // initial
}

/* ----------------------------------------------------------
   VIDEO — play/pause toggle on overlay click
---------------------------------------------------------- */
function initVideo() {
  const video   = document.querySelector('.video-frame video');
  const overlay = document.getElementById('videoOverlay');
  const playBtn = document.getElementById('playBtn');
  if (!video || !overlay || !playBtn) return;

  const updateIcon = () => {
    const icon = video.paused ? 'play' : 'pause';
    playBtn.innerHTML = `<i data-lucide="${icon}"></i>`;
    if (typeof lucide !== 'undefined') lucide.createIcons();
    // Show overlay when paused, hide when playing
    overlay.classList.toggle('visible', video.paused);
  };

  // Autoplay is muted — show overlay briefly then hide
  video.addEventListener('canplay', () => {
    updateIcon();
    // If autoplay succeeded, hide overlay after short delay
    if (!video.paused) {
      setTimeout(() => overlay.classList.remove('visible'), 1200);
    }
  });

  playBtn.addEventListener('click', () => {
    if (video.paused) {
      video.play();
      overlay.classList.remove('visible');
    } else {
      video.pause();
      overlay.classList.add('visible');
    }
    updateIcon();
  });

  // Show overlay on hover when paused
  video.addEventListener('pause', updateIcon);
  video.addEventListener('play',  updateIcon);
}

/* ----------------------------------------------------------
   FAQ — native <details> with icon rotation handled in CSS
   We just ensure Lucide icons are refreshed after open/close
---------------------------------------------------------- */
function initFAQ() {
  document.querySelectorAll('.faq-item').forEach(item => {
    item.addEventListener('toggle', () => {
      // Re-render icon so rotation applies to the live SVG
      if (typeof lucide !== 'undefined') lucide.createIcons();
    });
  });
}

/* ----------------------------------------------------------
   AUTH STATE — swap CTA if already logged in
---------------------------------------------------------- */
function initAuthState() {
  if (typeof Auth === 'undefined' || !Auth.isLoggedIn()) return;
  const user = Auth.user();
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
