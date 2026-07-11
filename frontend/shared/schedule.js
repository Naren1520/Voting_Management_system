/**
 * schedule.js - timezone-aware scheduling
 * - Searchable timezone card (no native <select>)
 * - Free-text hour:minute inputs + AM/PM toggle buttons
 * - All storage in UTC ISO; display in selected timezone, 12hr AM/PM
 */

const Schedule = (() => {
  const DAYS     = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  const POPULAR  = [
    'Asia/Kolkata','Asia/Dubai','Asia/Singapore','Asia/Tokyo',
    'Asia/Shanghai','Asia/Seoul','Asia/Bangkok','Asia/Karachi',
    'Asia/Dhaka','Asia/Colombo','Asia/Kathmandu','Asia/Almaty',
    'Europe/London','Europe/Paris','Europe/Berlin','Europe/Moscow',
    'America/New_York','America/Chicago','America/Denver','America/Los_Angeles',
    'America/Sao_Paulo','America/Toronto','America/Mexico_City',
    'Africa/Cairo','Africa/Nairobi','Africa/Lagos',
    'Australia/Sydney','Australia/Melbourne','Pacific/Auckland','Pacific/Honolulu'
  ];

  /* ── Timezone utilities ─────────────────────────────── */
  function userTZ() {
    return Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC';
  }

  function allTZ() {
    try {
      const all  = Intl.supportedValuesOf('timeZone');
      const pop  = POPULAR.filter(t => all.includes(t));
      const rest = all.filter(t => !pop.includes(t));
      return { popular: pop, rest };
    } catch {
      return { popular: POPULAR, rest: [] };
    }
  }

  function tzOffset(tz) {
    try {
      const now  = new Date();
      const utcS = now.toLocaleString('en-US', { timeZone: 'UTC' });
      const tzS  = now.toLocaleString('en-US', { timeZone: tz });
      const diff = (new Date(tzS) - new Date(utcS)) / 60000;
      const sign = diff >= 0 ? '+' : '-';
      const abs  = Math.abs(diff);
      return `UTC${sign}${pad(Math.floor(abs/60))}:${pad(abs%60)}`;
    } catch { return 'UTC'; }
  }

  function tzLabel(tz) {
    return tz.replace(/_/g,' ');
  }

  /* ── Time conversion ────────────────────────────────── */
  function naiveToUTC(dateStr, h, m, period, tz) {
    let hour = parseInt(h) || 0;
    if (period === 'AM' && hour === 12) hour = 0;
    if (period === 'PM' && hour !== 12) hour += 12;
    const time24 = `${pad(hour)}:${pad(parseInt(m)||0)}`;
    const naive  = new Date(`${dateStr}T${time24}:00Z`);
    const offset = getTZOffsetAt(naive, tz);
    return new Date(naive.getTime() - offset * 60000).toISOString();
  }

  function getTZOffsetAt(date, tz) {
    const u = new Date(date.toLocaleString('en-US',{timeZone:'UTC'}));
    const t = new Date(date.toLocaleString('en-US',{timeZone:tz}));
    return (t - u) / 60000;
  }

  function utcToTZParts(iso, tz) {
    const d = new Date(iso);
    const p = new Intl.DateTimeFormat('en-US', {
      timeZone: tz, year:'numeric', month:'2-digit', day:'2-digit',
      hour:'numeric', minute:'2-digit', hour12: true
    }).formatToParts(d);
    const get = t => p.find(x => x.type===t)?.value || '';
    return {
      date:   `${get('year')}-${get('month')}-${get('day')}`,
      hour:   get('hour'),
      minute: get('minute'),
      period: get('dayPeriod').toUpperCase() // 'AM' or 'PM'
    };
  }

  function fmtDt(iso, tz) {
    return new Date(iso).toLocaleString('en-US', {
      timeZone: tz || userTZ(),
      day:'numeric', month:'short', year:'numeric',
      hour:'numeric', minute:'2-digit', hour12: true
    });
  }

  function time24toAMPM(t24) {
    const [h, m] = t24.split(':').map(Number);
    const period = h < 12 ? 'AM' : 'PM';
    const h12    = h === 0 ? 12 : h > 12 ? h - 12 : h;
    return { hour: String(h12), minute: pad(m), period };
  }

  function ampmTo24(hour, minute, period) {
    let h = parseInt(hour) || 0;
    if (period === 'AM' && h === 12) h = 0;
    if (period === 'PM' && h !== 12) h += 12;
    return `${pad(h)}:${pad(parseInt(minute)||0)}`;
  }

  /* ── Widget state ───────────────────────────────────── */
  const _state = {}; // cid → { tz, tzPanelOpen }

  function state(cid) {
    if (!_state[cid]) _state[cid] = { tz: userTZ(), tzPanelOpen: false };
    return _state[cid];
  }

  /* ── Build full widget ──────────────────────────────── */
  function buildWidget(cid) {
    const wrap = document.getElementById(cid);
    if (!wrap) return;
    const s  = state(cid);
    const tz = s.tz;
    wrap.innerHTML = `
      <div class="sched-section">
        <div class="sched-section-title">
          <svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="18" rx="2"/>
            <line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/>
            <line x1="3" y1="10" x2="21" y2="10"/></svg>
          Schedule
        </div>

        ${_tzCardHTML(cid, tz)}
        ${_tzPanelHTML(cid, tz)}

        <div class="sched-type-row">
          <button type="button" class="sched-type-btn active" data-type="always_on"
            onclick="Schedule._setType('${cid}','always_on')">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
              <polyline points="12 6 12 12 16 14"/></svg>Always open
          </button>
          <button type="button" class="sched-type-btn" data-type="date_range"
            onclick="Schedule._setType('${cid}','date_range')">
            <svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="18" rx="2"/>
              <line x1="16" y1="2" x2="16" y2="6"/><line x1="8" y1="2" x2="8" y2="6"/>
              <line x1="3" y1="10" x2="21" y2="10"/></svg>Date range
          </button>
          <button type="button" class="sched-type-btn" data-type="recurring"
            onclick="Schedule._setType('${cid}','recurring')">
            <svg viewBox="0 0 24 24">
              <polyline points="23 4 23 10 17 10"/><polyline points="1 20 1 14 7 14"/>
              <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>
            </svg>Recurring days
          </button>
        </div>

        <div id="${cid}_dateRange" style="display:none;">
          <div class="sched-date-grid">
            <div class="sched-field">
              <label>Start date</label>
              <input type="date" class="sched-input" id="${cid}_startDate"/>
            </div>
            <div class="sched-field">
              <label>End date</label>
              <input type="date" class="sched-input" id="${cid}_endDate"/>
            </div>
          </div>
          <div class="sched-date-grid" style="margin-top:12px;">
            ${_timePicker(cid,'sT','Opens at','9','00','AM')}
            ${_timePicker(cid,'eT','Closes at','5','00','PM')}
          </div>
          <div class="sched-note" style="margin-top:12px;">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
              <line x1="12" y1="8" x2="12" y2="12"/>
              <line x1="12" y1="16" x2="12.01" y2="16"/></svg>
            Times are in the selected timezone.
          </div>
        </div>

        <div id="${cid}_recurring" style="display:none;">
          <!-- Compact date picker trigger -->
          <div class="sched-field" style="margin-bottom:14px;">
            <label>Voting dates</label>
            <button type="button" class="sched-datepick-trigger"
              id="${cid}_calTrigger"
              onclick="Schedule._toggleCal('${cid}')">
              <svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="18" rx="2"/>
                <line x1="16" y1="2" x2="16" y2="6"/>
                <line x1="8" y1="2" x2="8" y2="6"/>
                <line x1="3" y1="10" x2="21" y2="10"/>
              </svg>
              <span id="${cid}_triggerLabel">Select dates</span>
              <svg class="sched-datepick-caret" viewBox="0 0 24 24">
                <polyline points="6 9 12 15 18 9"/>
              </svg>
            </button>
            <!-- Calendar popover - centered overlay -->
            <div class="sched-cal-popover" id="${cid}_calPopover"
                 onclick="Schedule._calBackdropClick('${cid}',event)">
              <div class="sched-cal-popover-card" onclick="event.stopPropagation()">
                <div class="sched-cal-popover-bar">
                  <div class="sched-cal-popover-title">Select voting dates</div>
                  <button type="button" class="sched-cal-close"
                    onclick="Schedule._toggleCal('${cid}')" title="Close">
                    <svg viewBox="0 0 24 24">
                      <line x1="18" y1="6" x2="6" y2="18"/>
                      <line x1="6" y1="6" x2="18" y2="18"/>
                    </svg>
                  </button>
                </div>
                <div class="sched-cal" id="${cid}_cal"></div>
              </div>
            </div>
            <!-- Selected chips -->
            <div class="sched-sel-dates" id="${cid}_selDates"></div>
          </div>
          <div class="sched-date-grid">
            ${_timePicker(cid,'rsT','Opens at (each day)','9','00','AM')}
            ${_timePicker(cid,'reT','Closes at (each day)','5','00','PM')}
          </div>
          <div class="sched-note" style="margin-top:12px;">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
              <line x1="12" y1="8" x2="12" y2="12"/>
              <line x1="12" y1="16" x2="12.01" y2="16"/></svg>
            Voting opens on selected dates between the specified times.
          </div>
        </div>
      </div>`;
    setTimeout(() => _renderCal(cid), 0);
  }

  function _tzCardHTML(cid, tz) {
    const offset = tzOffset(tz);
    return `
      <div class="sched-tz-card" onclick="Schedule._toggleTZPanel('${cid}')">
        <div class="sched-tz-card-icon">
          <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
            <line x1="2" y1="12" x2="22" y2="12"/>
            <path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>
          </svg>
        </div>
        <div class="sched-tz-card-body">
          <div class="sched-tz-card-label">Timezone</div>
          <div class="sched-tz-card-value" id="${cid}_tzLabel">${tzLabel(tz)}</div>
          <div class="sched-tz-card-offset" id="${cid}_tzOffset">${offset}</div>
        </div>
        <div class="sched-tz-card-edit">
          <svg viewBox="0 0 24 24" style="width:14px;height:14px;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;vertical-align:-2px;margin-right:3px"><polyline points="6 9 12 15 18 9"/></svg>
          Change
        </div>
      </div>`;
  }

  function _tzPanelHTML(cid, selected) {
    const { popular, rest } = allTZ();
    const renderOpts = (list) => list.map(tz => `
      <div class="sched-tz-option ${tz===selected?'selected':''}"
           onclick="Schedule._selectTZ('${cid}','${tz}')">
        <span>${tzLabel(tz)}</span>
        <span class="sched-tz-option-offset">${tzOffset(tz)}</span>
      </div>`).join('');

    return `
      <!-- Timezone modal overlay - centered, same pattern as calendar popover -->
      <div class="sched-tz-popover" id="${cid}_tzPanel"
           onclick="Schedule._tzBackdropClick('${cid}',event)">
        <div class="sched-tz-popover-card" onclick="event.stopPropagation()">
          <div class="sched-tz-popover-bar">
            <div class="sched-tz-popover-title">Select timezone</div>
            <button type="button" class="sched-cal-close"
              onclick="Schedule._toggleTZPanel('${cid}')" title="Close">
              <svg viewBox="0 0 24 24">
                <line x1="18" y1="6" x2="6" y2="18"/>
                <line x1="6" y1="6" x2="18" y2="18"/>
              </svg>
            </button>
          </div>
          <div class="sched-tz-search-wrap">
            <svg viewBox="0 0 24 24"><circle cx="11" cy="11" r="8"/>
              <line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
            <input class="sched-tz-search" id="${cid}_tzSearch"
              placeholder="Search timezone…"
              oninput="Schedule._filterTZ('${cid}', this.value)"/>
          </div>
          <div class="sched-tz-list" id="${cid}_tzList">
            <div class="sched-tz-group-label">Popular</div>
            ${renderOpts(popular)}
            <div class="sched-tz-group-label">All timezones</div>
            ${renderOpts(rest)}
          </div>
        </div>
      </div>`;
  }

  function _timePicker(cid, sfx, label, defH, defM, defP) {
    return `
      <div class="sched-field">
        <label>${label}</label>
        <div class="sched-time-picker">
          <input class="sched-time-input" id="${cid}_${sfx}_h"
            type="text" inputmode="numeric" maxlength="2"
            placeholder="12" value="${defH}"
            oninput="Schedule._clampHour(this)"/>
          <span class="sched-time-sep">:</span>
          <input class="sched-time-input" id="${cid}_${sfx}_m"
            type="text" inputmode="numeric" maxlength="2"
            placeholder="00" value="${defM}"
            oninput="Schedule._clampMin(this)"/>
          <div class="sched-ampm">
            <button type="button" class="sched-ampm-btn ${defP==='AM'?'active':''}"
              id="${cid}_${sfx}_am"
              onclick="Schedule._setPeriod('${cid}','${sfx}','AM')">AM</button>
            <button type="button" class="sched-ampm-btn ${defP==='PM'?'active':''}"
              id="${cid}_${sfx}_pm"
              onclick="Schedule._setPeriod('${cid}','${sfx}','PM')">PM</button>
          </div>
        </div>
      </div>`;
  }

  /* ── Calendar state + render ────────────────────────── */
  function _calState(cid) {
    const s = state(cid);
    if (!s.cal) {
      const now = new Date();
      s.cal = { year: now.getFullYear(), month: now.getMonth(), dates: new Set() };
    }
    return s.cal;
  }

  function _renderCal(cid) {
    const wrap = document.getElementById(`${cid}_cal`);
    if (!wrap) return;
    const cs    = _calState(cid);
    const year  = cs.year;
    const month = cs.month;
    const today = _dateStr(new Date());

    const MONTH_NAMES = ['January','February','March','April','May','June',
                         'July','August','September','October','November','December'];
    const DAY_NAMES   = ['Su','Mo','Tu','We','Th','Fr','Sa'];

    const firstDay  = new Date(year, month, 1).getDay();
    const totalDays = new Date(year, month+1, 0).getDate();

    let cells = '';
    for (let i = 0; i < firstDay; i++)
      cells += `<div class="sched-cal-cell sched-cal-empty"></div>`;
    for (let d = 1; d <= totalDays; d++) {
      const ds      = `${year}-${pad(month+1)}-${pad(d)}`;
      const isSel   = cs.dates.has(ds);
      const isToday = ds === today;
      const isPast  = ds < today;
      cells += `
        <div class="sched-cal-cell ${isSel?'selected':''} ${isToday?'today':''} ${isPast?'past':''}"
             onclick="event.stopPropagation(); Schedule._toggleDate('${cid}','${ds}')">${d}</div>`;
    }

    wrap.innerHTML = `
      <div class="sched-cal-header">
        <button type="button" class="sched-cal-nav"
          onclick="event.stopPropagation(); Schedule._calNav('${cid}',-1)">
          <svg viewBox="0 0 24 24"><polyline points="15 18 9 12 15 6"/></svg>
        </button>
        <span class="sched-cal-title">${MONTH_NAMES[month]} ${year}</span>
        <button type="button" class="sched-cal-nav"
          onclick="event.stopPropagation(); Schedule._calNav('${cid}',1)">
          <svg viewBox="0 0 24 24"><polyline points="9 18 15 12 9 6"/></svg>
        </button>
      </div>
      <div class="sched-cal-grid">
        ${DAY_NAMES.map(d=>`<div class="sched-cal-day-name">${d}</div>`).join('')}
        ${cells}
      </div>`;

    _renderSelDates(cid);
  }

  function _calNav(cid, dir) {
    const cs = _calState(cid);
    cs.month += dir;
    if (cs.month > 11) { cs.month = 0;  cs.year++; }
    if (cs.month < 0)  { cs.month = 11; cs.year--; }
    _renderCal(cid);
  }

  function _toggleCal(cid) {
    const pop = document.getElementById(`${cid}_calPopover`);
    if (!pop) return;
    const opening = !pop.classList.contains('open');
    pop.classList.toggle('open', opening);
    if (opening) _renderCal(cid);
  }

  // Close ONLY when clicking directly on the backdrop (not the card or anything inside it)
  function _calBackdropClick(cid, event) {
    if (event.target === event.currentTarget) {
      document.getElementById(`${cid}_calPopover`)?.classList.remove('open');
    }
  }

  function _toggleDate(cid, ds) {
    const cs = _calState(cid);
    if (cs.dates.has(ds)) cs.dates.delete(ds);
    else                   cs.dates.add(ds);
    _renderCal(cid);
  }

  function _renderSelDates(cid) {
    const wrap = document.getElementById(`${cid}_selDates`);
    const lbl  = document.getElementById(`${cid}_triggerLabel`);
    const dates = [..._calState(cid).dates].sort();

    // Update trigger label
    if (lbl) {
      lbl.textContent = dates.length === 0
        ? 'Select dates'
        : dates.length === 1
          ? fmtSelDate(dates[0])
          : `${dates.length} dates selected`;
    }

    if (!wrap) return;
    if (!dates.length) {
      wrap.innerHTML = '';
      return;
    }
    wrap.innerHTML = dates.map(d => `
      <span class="sched-sel-chip">
        ${fmtSelDate(d)}
        <button type="button" onclick="Schedule._toggleDate('${cid}','${d}')">
          <svg viewBox="0 0 24 24"><line x1="18" y1="6" x2="6" y2="18"/>
            <line x1="6" y1="6" x2="18" y2="18"/></svg>
        </button>
      </span>`).join('');
  }

  function fmtSelDate(ds) {
    return new Date(ds + 'T12:00:00Z').toLocaleDateString('en-US',
      { month:'short', day:'numeric', year:'numeric', timeZone:'UTC' });
  }

  function _dateStr(d) {
    return `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())}`;
  }

  /* ── UI event handlers ──────────────────────────────── */
  function _toggleTZPanel(cid) {
    const panel = document.getElementById(`${cid}_tzPanel`);
    if (!panel) return;
    const opening = !panel.classList.contains('open');
    panel.classList.toggle('open', opening);
    if (opening) {
      // Clear previous search
      const search = document.getElementById(`${cid}_tzSearch`);
      if (search) { search.value = ''; _filterTZ(cid, ''); }
      setTimeout(() => search?.focus(), 50);
      // Scroll selected into view
      const sel = panel.querySelector('.sched-tz-option.selected');
      if (sel) setTimeout(() => sel.scrollIntoView({ block: 'nearest' }), 60);
    }
  }

  // Close ONLY when clicking directly on the backdrop
  function _tzBackdropClick(cid, event) {
    if (event.target === event.currentTarget) {
      document.getElementById(`${cid}_tzPanel`)?.classList.remove('open');
    }
  }

  function _selectTZ(cid, tz) {
    state(cid).tz = tz;
    // Update card display
    const lbl = document.getElementById(`${cid}_tzLabel`);
    const off = document.getElementById(`${cid}_tzOffset`);
    if (lbl) lbl.textContent = tzLabel(tz);
    if (off) off.textContent = tzOffset(tz);
    // Update selected in list
    document.querySelectorAll(`#${cid}_tzList .sched-tz-option`).forEach(el => {
      el.classList.toggle('selected', el.textContent.trim().startsWith(tzLabel(tz)));
    });
    // Close modal
    document.getElementById(`${cid}_tzPanel`)?.classList.remove('open');
  }

  function _filterTZ(cid, query) {
    const q   = query.toLowerCase().replace(/\s/g,'_');
    const all = document.querySelectorAll(`#${cid}_tzList .sched-tz-option`);
    const groups = document.querySelectorAll(`#${cid}_tzList .sched-tz-group-label`);
    let anyVisible = false;
    all.forEach(el => {
      const match = el.textContent.toLowerCase().includes(q) ||
                    el.dataset.tz?.toLowerCase().includes(q);
      el.style.display = match ? '' : 'none';
      if (match) anyVisible = true;
    });
    groups.forEach(g => g.style.display = q ? 'none' : '');
    // Show empty state
    let empty = document.getElementById(`${cid}_tzEmpty`);
    if (!anyVisible && q) {
      if (!empty) {
        empty = document.createElement('div');
        empty.id = `${cid}_tzEmpty`;
        empty.className = 'sched-tz-empty';
        empty.textContent = 'No timezones found';
        document.getElementById(`${cid}_tzList`)?.appendChild(empty);
      }
      empty.style.display = '';
    } else if (empty) {
      empty.style.display = 'none';
    }
  }

  function _setType(cid, type) {
    const wrap = document.getElementById(cid);
    if (!wrap) return;
    wrap.querySelectorAll('.sched-type-btn').forEach(b =>
      b.classList.toggle('active', b.dataset.type === type));
    const dr = document.getElementById(`${cid}_dateRange`);
    const rc = document.getElementById(`${cid}_recurring`);
    if (dr) dr.style.display = type === 'date_range' ? 'block' : 'none';
    if (rc) rc.style.display = type === 'recurring'  ? 'block' : 'none';
    if (type === 'recurring') _renderCal(cid);
  }

  function _toggleDay(cid, btn) { btn.classList.toggle('active'); } // legacy no-op

  function _setPeriod(cid, sfx, period) {
    document.getElementById(`${cid}_${sfx}_am`)?.classList.toggle('active', period==='AM');
    document.getElementById(`${cid}_${sfx}_pm`)?.classList.toggle('active', period==='PM');
  }

  function _clampHour(input) {
    let v = input.value.replace(/\D/g,'');
    if (v.length > 2) v = v.slice(-2);
    const n = parseInt(v);
    if (!isNaN(n) && n > 12) v = '12';
    if (!isNaN(n) && n < 1 && v.length >= 1 && v !== '0') v = '1';
    input.value = v;
    if (v.length === 2) {
      // Auto-advance to minute field
      const next = input.nextElementSibling?.nextElementSibling;
      if (next && next.classList.contains('sched-time-input')) next.focus();
    }
  }

  function _clampMin(input) {
    let v = input.value.replace(/\D/g,'');
    if (v.length > 2) v = v.slice(-2);
    const n = parseInt(v);
    if (!isNaN(n) && n > 59) v = '59';
    input.value = v;
  }

  /* ── Get / Set value ────────────────────────────────── */
  function _getTimeParts(cid, sfx) {
    const h = (document.getElementById(`${cid}_${sfx}_h`)?.value || '12').trim() || '12';
    const m = (document.getElementById(`${cid}_${sfx}_m`)?.value || '00').trim() || '00';
    const p = document.getElementById(`${cid}_${sfx}_am`)?.classList.contains('active') ? 'AM' : 'PM';
    return { h, m, p };
  }

  function _setTimeParts(cid, sfx, hour, minute, period) {
    const hEl = document.getElementById(`${cid}_${sfx}_h`);
    const mEl = document.getElementById(`${cid}_${sfx}_m`);
    if (hEl) hEl.value = hour;
    if (mEl) mEl.value = pad(parseInt(minute)||0);
    _setPeriod(cid, sfx, period.toUpperCase());
  }

  function getValue(cid) {
    const wrap = document.getElementById(cid);
    if (!wrap) return { schedule_type: 'always_on' };
    const type = wrap.querySelector('.sched-type-btn.active')?.dataset.type || 'always_on';
    const tz   = state(cid).tz || userTZ();

    if (type === 'date_range') {
      const sd = document.getElementById(`${cid}_startDate`)?.value || '';
      const ed = document.getElementById(`${cid}_endDate`)?.value   || '';
      const st = _getTimeParts(cid, 'sT');
      const et = _getTimeParts(cid, 'eT');
      return {
        schedule_type: 'date_range', timezone: tz,
        starts_at: sd ? naiveToUTC(sd, st.h, st.m, st.p, tz) : null,
        ends_at:   ed ? naiveToUTC(ed, et.h, et.m, et.p, tz) : null
      };
    }
    if (type === 'recurring') {
      const dates = [...(_calState(cid).dates)].sort();
      const st = _getTimeParts(cid, 'rsT');
      const et = _getTimeParts(cid, 'reT');
      return {
        schedule_type: 'recurring', timezone: tz,
        schedule_json: {
          dates,
          start_time: ampmTo24(st.h, st.m, st.p),
          end_time:   ampmTo24(et.h, et.m, et.p)
        }
      };
    }
    return { schedule_type: 'always_on', timezone: tz };
  }

  function setValue(cid, sched) {
    if (!sched) return;
    if (sched.timezone) {
      state(cid).tz = sched.timezone;
      _selectTZ(cid, sched.timezone);
    }
    _setType(cid, sched.schedule_type || 'always_on');
    const tz = state(cid).tz;

    if (sched.schedule_type === 'date_range') {
      if (sched.starts_at) {
        const p = utcToTZParts(sched.starts_at, tz);
        const sd = document.getElementById(`${cid}_startDate`);
        if (sd) sd.value = p.date;
        _setTimeParts(cid, 'sT', p.hour, p.minute, p.period);
      }
      if (sched.ends_at) {
        const p = utcToTZParts(sched.ends_at, tz);
        const ed = document.getElementById(`${cid}_endDate`);
        if (ed) ed.value = p.date;
        _setTimeParts(cid, 'eT', p.hour, p.minute, p.period);
      }
    }
    if (sched.schedule_type === 'recurring' && sched.schedule_json) {
      const j  = sched.schedule_json;
      const cs = _calState(cid);
      cs.dates = new Set(j.dates || []);
      if (j.start_time) { const s=time24toAMPM(j.start_time); _setTimeParts(cid,'rsT',s.hour,s.minute,s.period); }
      if (j.end_time)   { const e=time24toAMPM(j.end_time);   _setTimeParts(cid,'reT',e.hour,e.minute,e.period); }
      _renderCal(cid);
    }
  }

  function validate(cid) {
    const v = getValue(cid);
    if (v.schedule_type === 'date_range') {
      if (!v.starts_at) return 'Please set a start date and time.';
      if (!v.ends_at)   return 'Please set an end date and time.';
      if (new Date(v.starts_at) >= new Date(v.ends_at))
        return 'End must be after start.';
    }
    if (v.schedule_type === 'recurring') {
      if (!v.schedule_json?.dates?.length) return 'Select at least one date.';
      if (v.schedule_json.start_time >= v.schedule_json.end_time)
        return 'Close time must be after open time.';
    }
    return null;
  }

  /* ── Status check ───────────────────────────────────── */
  function getStatus(sched) {
    if (!sched || sched.schedule_type === 'always_on')
      return { open: true, label: 'Always open', reason: '', nextChange: null };

    const tz  = sched.timezone || userTZ();
    const now = new Date();

    if (sched.schedule_type === 'date_range') {
      const start = sched.starts_at ? new Date(sched.starts_at) : null;
      const end   = sched.ends_at   ? new Date(sched.ends_at)   : null;
      if (start && now < start)
        return { open:false, label:'Not started yet',  reason:'Opens '  +fmtDt(start,tz), nextChange:start };
      if (end && now > end)
        return { open:false, label:'Voting closed',    reason:'Closed ' +fmtDt(end,tz),   nextChange:null  };
      return { open:true, label:'Voting open', reason: end ? 'Closes '+fmtDt(end,tz) : '', nextChange:end };
    }

    if (sched.schedule_type === 'recurring') {
      const j      = sched.schedule_json || {};
      const dates  = j.dates      || [];
      const startT = j.start_time || '09:00';
      const endT   = j.end_time   || '17:00';

      // Today's date string in the schedule's timezone
      const todayInTZ = new Intl.DateTimeFormat('en-CA', {
        timeZone: tz, year:'numeric', month:'2-digit', day:'2-digit'
      }).format(now); // "YYYY-MM-DD"

      const nowInTZ   = new Date(now.toLocaleString('en-US', { timeZone: tz }));
      const nowMins   = nowInTZ.getHours()*60 + nowInTZ.getMinutes();
      const startMins = toMins(startT);
      const endMins   = toMins(endT);
      const todayOpen = dates.includes(todayInTZ) && nowMins>=startMins && nowMins<endMins;

      const s12 = fmt12(startT);
      const e12 = fmt12(endT);
      const upcoming = dates.filter(d => d > todayInTZ).sort();

      if (todayOpen) {
        const close = closeUTC(now, endT, tz);
        return { open:true, label:'Voting open',
          reason:`Open today until ${e12} (${tz})`, nextChange:close };
      }

      // Check if today is a voting day but before open time
      if (dates.includes(todayInTZ) && nowMins < startMins) {
        const open = naiveToUTC(todayInTZ, ...startT.split(':'), '', tz);
        return { open:false, label:'Not started yet today',
          reason:`Opens today at ${s12} (${tz})`,
          nextChange: new Date(open) };
      }

      // Next upcoming date
      if (upcoming.length) {
        const nextDate = upcoming[0];
        const nextOpen = new Date(naiveToUTC(nextDate, ...startT.split(':'), '', tz));
        return { open:false, label:'Outside voting hours',
          reason:`Next: ${fmtSelDate(nextDate)} at ${s12} (${tz})`,
          nextChange: nextOpen };
      }

      return { open:false, label:'Voting closed',
        reason:'All scheduled dates have passed', nextChange:null };
    }
    return { open:true, label:'Always open', reason:'', nextChange:null };
  }

  function closeUTC(now, endT, tz) {
    const tzNow = new Date(now.toLocaleString('en-US',{timeZone:tz}));
    const ds = `${tzNow.getFullYear()}-${pad(tzNow.getMonth()+1)}-${pad(tzNow.getDate())}`;
    return new Date(naiveToUTC(ds, ...endT.split(':').map(Number), '', tz));
  }

  /* ── Status banner ──────────────────────────────────── */
  function renderStatusBanner(sched, el) {
    if (!el) return;
    const s   = getStatus(sched);
    const cls = s.open ? 'open' : (s.nextChange ? 'upcoming' : 'closed');
    const uid = Math.random().toString(36).slice(2,8);
    el.innerHTML = `
      <div class="sched-status-banner ${cls}">
        <div class="sched-status-dot"></div>
        <div class="sched-status-text">
          <strong>${s.label}</strong>
          <span>${s.reason}</span>
        </div>
        ${s.nextChange ? `<div class="sched-countdown" id="scd_${uid}"></div>` : ''}
      </div>`;
    if (s.nextChange) startCountdown(s.nextChange, el.querySelector(`#scd_${uid}`));
  }

  function startCountdown(target, el) {
    if (!el || !target) return;
    (function tick() {
      const diff = new Date(target) - new Date();
      if (diff <= 0) { el.textContent = 'Opening now…'; return; }
      const h = Math.floor(diff/3600000);
      const m = Math.floor((diff%3600000)/60000);
      const s = Math.floor((diff%60000)/1000);
      el.textContent = h>0 ? `${h}h ${pad(m)}m ${pad(s)}s` : `${pad(m)}m ${pad(s)}s`;
      setTimeout(tick, 1000);
    })();
  }

  /* ── Tiny helpers ───────────────────────────────────── */
  function pad(n) { return String(n).padStart(2,'0'); }
  function toMins(t) { const [h,m]=t.split(':').map(Number); return h*60+m; }
  function fmt12(t24) {
    const {hour,minute,period} = time24toAMPM(t24);
    return `${hour}:${pad(parseInt(minute))} ${period}`;
  }
  function time24toAMPM(t24) {
    const [h,m] = t24.split(':').map(Number);
    const p = h<12?'AM':'PM';
    return { hour: String(h===0?12:h>12?h-12:h), minute: pad(m), period: p };
  }
  function ampmTo24(hour, minute, period) {
    let h = parseInt(hour)||0;
    if (period==='AM' && h===12) h=0;
    if (period==='PM' && h!==12) h+=12;
    return `${pad(h)}:${pad(parseInt(minute)||0)}`;
  }

  return {
    buildWidget, getValue, setValue, validate,
    getStatus, renderStatusBanner, startCountdown,
    _setType, _toggleDay, _toggleTZPanel, _selectTZ,
    _filterTZ, _setPeriod, _clampHour, _clampMin,
    _calNav, _toggleDate, _renderCal, _toggleCal, _calBackdropClick,
    _tzBackdropClick
  };
})();
