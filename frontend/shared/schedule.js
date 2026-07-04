/**
 * schedule.js — timezone-aware scheduling utilities
 * Uses browser-native Intl API — no external dependencies.
 *
 * All times stored in DB as UTC ISO strings.
 * All display uses the elected timezone in 12-hour AM/PM format.
 *
 * Window.Schedule exports:
 *   buildWidget(cid, opts)       — render picker UI
 *   getValue(cid)                — get schedule object (UTC ISO)
 *   setValue(cid, sched)         — populate picker from saved schedule
 *   validate(cid)                — returns error string or null
 *   getStatus(sched, tz)         — {open, label, reason, nextChange}
 *   renderStatusBanner(sched, el)— inject status banner
 *   startCountdown(target, el)   — live countdown
 */

const Schedule = (() => {

  const DAYS      = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
  const HOURS_12  = ['12','1','2','3','4','5','6','7','8','9','10','11'];
  const MINUTES   = ['00','15','30','45'];
  const PERIODS   = ['AM','PM'];

  /* ── Popular timezones shown first ─────────────────── */
  const POPULAR_TZ = [
    'Asia/Kolkata','Asia/Dubai','Asia/Singapore','Asia/Tokyo',
    'Asia/Shanghai','Asia/Seoul','Asia/Bangkok','Asia/Karachi',
    'Asia/Dhaka','Asia/Colombo',
    'Europe/London','Europe/Paris','Europe/Berlin','Europe/Moscow',
    'America/New_York','America/Chicago','America/Denver',
    'America/Los_Angeles','America/Sao_Paulo',
    'Africa/Cairo','Africa/Nairobi',
    'Australia/Sydney','Pacific/Auckland'
  ];

  function getAllTimezones() {
    try {
      const all = Intl.supportedValuesOf('timeZone');
      const popular = POPULAR_TZ.filter(tz => all.includes(tz));
      const rest    = all.filter(tz => !popular.includes(tz));
      return { popular, rest };
    } catch {
      return { popular: POPULAR_TZ, rest: [] };
    }
  }

  function userTZ() {
    return Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC';
  }

  /* ── Time conversion helpers ────────────────────────── */

  // "2026-07-05T12:00" (local naive) + timezone → UTC ISO string
  function naiveLocalToUTC(dateStr, timeStr, tz) {
    // dateStr: "2026-07-05", timeStr: "13:30" (24hr internal)
    const iso = `${dateStr}T${timeStr}:00`;
    // Use Temporal if available, else manual offset approach
    try {
      // Create a date in the target timezone via formatting trick
      const formatter = new Intl.DateTimeFormat('en-CA', {
        timeZone: tz,
        year: 'numeric', month: '2-digit', day: '2-digit',
        hour: '2-digit', minute: '2-digit', second: '2-digit',
        hour12: false
      });
      // Binary search isn't needed — use the offset at that moment
      // Parse the naive ISO as if it were UTC, then adjust by the offset
      const naive    = new Date(iso + 'Z');           // treat as UTC first
      const offset   = getTZOffsetAt(naive, tz);      // offset in minutes
      const adjusted = new Date(naive.getTime() - offset * 60000);
      return adjusted.toISOString();
    } catch {
      return new Date(iso).toISOString();
    }
  }

  // Returns timezone offset in minutes (positive = ahead of UTC)
  function getTZOffsetAt(date, tz) {
    const utcStr  = date.toLocaleString('en-US', { timeZone: 'UTC' });
    const tzStr   = date.toLocaleString('en-US', { timeZone: tz });
    const utcD    = new Date(utcStr);
    const tzD     = new Date(tzStr);
    return (tzD - utcD) / 60000;
  }

  // UTC ISO → { date: "2026-07-05", time24: "13:30" } in given timezone
  function utcToTZParts(isoStr, tz) {
    const d = new Date(isoStr);
    const parts = new Intl.DateTimeFormat('en-CA', {
      timeZone: tz,
      year: 'numeric', month: '2-digit', day: '2-digit',
      hour: '2-digit', minute: '2-digit', hour12: false
    }).formatToParts(d);
    const get = t => parts.find(p => p.type === t)?.value || '00';
    return {
      date:   `${get('year')}-${get('month')}-${get('day')}`,
      time24: `${get('hour').replace('24','00')}:${get('minute')}`
    };
  }

  // "13:30" → { hour12: "1", minute: "30", period: "PM" }
  function time24to12(t24) {
    const [h, m] = t24.split(':').map(Number);
    const period = h < 12 ? 'AM' : 'PM';
    const h12    = h === 0 ? 12 : h > 12 ? h - 12 : h;
    return { hour12: String(h12), minute: String(m).padStart(2,'0'), period };
  }

  // { hour12, minute, period } → "13:30"
  function time12to24(hour12, minute, period) {
    let h = parseInt(hour12);
    if (period === 'AM' && h === 12) h = 0;
    if (period === 'PM' && h !== 12) h += 12;
    return `${String(h).padStart(2,'0')}:${minute}`;
  }

  // Format UTC ISO for display in given tz, 12hr AM/PM
  function fmtDt(isoStr, tz) {
    return new Date(isoStr).toLocaleString('en-US', {
      timeZone: tz || userTZ(),
      day: 'numeric', month: 'short', year: 'numeric',
      hour: 'numeric', minute: '2-digit',
      hour12: true
    });
  }

  // Format time-only in 12hr
  function fmtTime(time24, tz) {
    const today = new Date().toISOString().split('T')[0];
    return new Date(`${today}T${time24}:00Z`).toLocaleString('en-US', {
      timeZone: tz || 'UTC',
      hour: 'numeric', minute: '2-digit', hour12: true
    });
  }

  /* ── Build timezone select HTML ────────────────────── */
  function tzSelectHTML(cid, selected) {
    const { popular, rest } = getAllTimezones();
    const sel = selected || userTZ();
    const opt = (tz) => `<option value="${tz}" ${tz===sel?'selected':''}>${tz.replace(/_/g,' ')}</option>`;
    return `
      <select class="sched-input sched-tz-select" id="${cid}_tz">
        <optgroup label="— Common timezones —">
          ${popular.map(opt).join('')}
        </optgroup>
        <optgroup label="— All timezones —">
          ${rest.map(opt).join('')}
        </optgroup>
      </select>`;
  }

  /* ── Build 12hr time picker HTML ───────────────────── */
  function timePicker12HTML(cid, suffix, label, defaultTime24) {
    const { hour12, minute, period } = time24to12(defaultTime24 || '09:00');
    return `
      <div class="sched-field">
        <label>${label}</label>
        <div class="sched-time12-wrap">
          <select class="sched-time12-part" id="${cid}_${suffix}_h">
            ${HOURS_12.map(h => `<option ${h===hour12?'selected':''}>${h}</option>`).join('')}
          </select>
          <span class="sched-time12-sep">:</span>
          <select class="sched-time12-part" id="${cid}_${suffix}_m">
            ${MINUTES.map(m => `<option ${m===minute?'selected':''}>${m}</option>`).join('')}
          </select>
          <select class="sched-time12-part sched-time12-period" id="${cid}_${suffix}_p">
            ${PERIODS.map(p => `<option ${p===period?'selected':''}>${p}</option>`).join('')}
          </select>
        </div>
      </div>`;
  }

  function getTime12Value(cid, suffix) {
    const h = document.getElementById(`${cid}_${suffix}_h`)?.value || '12';
    const m = document.getElementById(`${cid}_${suffix}_m`)?.value || '00';
    const p = document.getElementById(`${cid}_${suffix}_p`)?.value || 'AM';
    return time12to24(h, m, p);
  }

  function setTime12Value(cid, suffix, time24) {
    if (!time24) return;
    const { hour12, minute, period } = time24to12(time24);
    const setVal = (id, val) => {
      const el = document.getElementById(id);
      if (el) el.value = val;
    };
    setVal(`${cid}_${suffix}_h`, hour12);
    setVal(`${cid}_${suffix}_m`, minute);
    setVal(`${cid}_${suffix}_p`, period);
  }

  /* ── Build widget ───────────────────────────────────── */
  function buildWidget(cid, opts = {}) {
    const wrap = document.getElementById(cid);
    if (!wrap) return;
    wrap.innerHTML = `
      <div class="sched-section">
        <div class="sched-section-title">
          <svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="18" rx="2"/>
            <line x1="16" y1="2" x2="16" y2="6"/>
            <line x1="8" y1="2" x2="8" y2="6"/>
            <line x1="3" y1="10" x2="21" y2="10"/>
          </svg>
          Schedule
        </div>

        <!-- Timezone selector -->
        <div class="sched-field" style="margin-bottom:16px;">
          <label>Timezone</label>
          ${tzSelectHTML(cid, opts.timezone)}
        </div>

        <!-- Type pills -->
        <div class="sched-type-row">
          <button type="button" class="sched-type-btn active" data-type="always_on"
            onclick="Schedule._setType('${cid}','always_on')">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
              <polyline points="12 6 12 12 16 14"/></svg>
            Always open
          </button>
          <button type="button" class="sched-type-btn" data-type="date_range"
            onclick="Schedule._setType('${cid}','date_range')">
            <svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="18" rx="2"/>
              <line x1="16" y1="2" x2="16" y2="6"/>
              <line x1="8" y1="2" x2="8" y2="6"/>
              <line x1="3" y1="10" x2="21" y2="10"/>
            </svg>
            Date range
          </button>
          <button type="button" class="sched-type-btn" data-type="recurring"
            onclick="Schedule._setType('${cid}','recurring')">
            <svg viewBox="0 0 24 24">
              <polyline points="23 4 23 10 17 10"/>
              <polyline points="1 20 1 14 7 14"/>
              <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"/>
            </svg>
            Recurring days
          </button>
        </div>

        <!-- Date range -->
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
            ${timePicker12HTML(cid, 'startTime', 'Opens at', '09:00')}
            ${timePicker12HTML(cid, 'endTime',   'Closes at','17:00')}
          </div>
          <div class="sched-note" style="margin-top:12px;">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
              <line x1="12" y1="8" x2="12" y2="12"/>
              <line x1="12" y1="16" x2="12.01" y2="16"/>
            </svg>
            Times are in the selected timezone.
          </div>
        </div>

        <!-- Recurring -->
        <div id="${cid}_recurring" style="display:none;">
          <div class="sched-field" style="margin-bottom:12px;">
            <label>Days voting is open</label>
            <div class="sched-days-row" id="${cid}_days">
              ${DAYS.map((d,i) => `
                <button type="button" class="sched-day-btn" data-day="${i}"
                  onclick="Schedule._toggleDay('${cid}',${i},this)">${d}</button>
              `).join('')}
            </div>
          </div>
          <div class="sched-date-grid">
            ${timePicker12HTML(cid, 'recStartTime', 'Opens at',  '09:00')}
            ${timePicker12HTML(cid, 'recEndTime',   'Closes at', '17:00')}
          </div>
          <div class="sched-note" style="margin-top:12px;">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="10"/>
              <line x1="12" y1="8" x2="12" y2="12"/>
              <line x1="12" y1="16" x2="12.01" y2="16"/>
            </svg>
            Times are in the selected timezone.
          </div>
        </div>
      </div>`;
  }

  /* ── Type / day toggles ─────────────────────────────── */
  function _setType(cid, type) {
    const wrap = document.getElementById(cid);
    if (!wrap) return;
    wrap.querySelectorAll('.sched-type-btn').forEach(b =>
      b.classList.toggle('active', b.dataset.type === type));
    const dr = document.getElementById(`${cid}_dateRange`);
    const rc = document.getElementById(`${cid}_recurring`);
    if (dr) dr.style.display = type === 'date_range' ? 'block' : 'none';
    if (rc) rc.style.display = type === 'recurring'  ? 'block' : 'none';
  }

  function _toggleDay(cid, dayIdx, btn) {
    btn.classList.toggle('active');
  }

  /* ── Get / Set value ────────────────────────────────── */
  function getValue(cid) {
    const wrap = document.getElementById(cid);
    if (!wrap) return { schedule_type: 'always_on' };

    const type = wrap.querySelector('.sched-type-btn.active')?.dataset.type || 'always_on';
    const tz   = document.getElementById(`${cid}_tz`)?.value || userTZ();

    if (type === 'date_range') {
      const startDate = document.getElementById(`${cid}_startDate`)?.value || '';
      const endDate   = document.getElementById(`${cid}_endDate`)?.value   || '';
      const startT24  = getTime12Value(cid, 'startTime');
      const endT24    = getTime12Value(cid, 'endTime');
      return {
        schedule_type: 'date_range',
        timezone:  tz,
        // Convert local date+time in selected tz → UTC ISO
        starts_at: startDate ? naiveLocalToUTC(startDate, startT24, tz) : null,
        ends_at:   endDate   ? naiveLocalToUTC(endDate,   endT24,   tz) : null
      };
    }

    if (type === 'recurring') {
      const days = [];
      wrap.querySelectorAll('.sched-day-btn.active').forEach(b =>
        days.push(parseInt(b.dataset.day)));
      return {
        schedule_type: 'recurring',
        timezone: tz,
        schedule_json: {
          days,
          start_time: getTime12Value(cid, 'recStartTime'),
          end_time:   getTime12Value(cid, 'recEndTime')
        }
      };
    }

    return { schedule_type: 'always_on', timezone: tz };
  }

  function setValue(cid, sched) {
    if (!sched) return;
    _setType(cid, sched.schedule_type || 'always_on');

    // Set timezone
    const tzEl = document.getElementById(`${cid}_tz`);
    if (tzEl && sched.timezone) tzEl.value = sched.timezone;

    const tz = document.getElementById(`${cid}_tz`)?.value || userTZ();

    if (sched.schedule_type === 'date_range') {
      if (sched.starts_at) {
        const { date, time24 } = utcToTZParts(sched.starts_at, tz);
        const sdEl = document.getElementById(`${cid}_startDate`);
        if (sdEl) sdEl.value = date;
        setTime12Value(cid, 'startTime', time24);
      }
      if (sched.ends_at) {
        const { date, time24 } = utcToTZParts(sched.ends_at, tz);
        const edEl = document.getElementById(`${cid}_endDate`);
        if (edEl) edEl.value = date;
        setTime12Value(cid, 'endTime', time24);
      }
    }

    if (sched.schedule_type === 'recurring' && sched.schedule_json) {
      const j = sched.schedule_json;
      const wrap = document.getElementById(cid);
      wrap?.querySelectorAll('.sched-day-btn').forEach(b =>
        b.classList.toggle('active', (j.days||[]).includes(parseInt(b.dataset.day))));
      setTime12Value(cid, 'recStartTime', j.start_time);
      setTime12Value(cid, 'recEndTime',   j.end_time);
    }
  }

  /* ── Validate ───────────────────────────────────────── */
  function validate(cid) {
    const v = getValue(cid);
    if (v.schedule_type === 'date_range') {
      if (!v.starts_at) return 'Please set a start date and time.';
      if (!v.ends_at)   return 'Please set an end date and time.';
      if (new Date(v.starts_at) >= new Date(v.ends_at))
        return 'End must be after start.';
    }
    if (v.schedule_type === 'recurring') {
      if (!v.schedule_json?.days?.length) return 'Select at least one day.';
      if (v.schedule_json.start_time >= v.schedule_json.end_time)
        return 'Close time must be after open time.';
    }
    return null;
  }

  /* ── Status check ───────────────────────────────────── */
  function getStatus(sched) {
    if (!sched || sched.schedule_type === 'always_on') {
      return { open: true, label: 'Always open', reason: '', nextChange: null };
    }

    const tz  = sched.timezone || userTZ();
    const now = new Date();

    if (sched.schedule_type === 'date_range') {
      const start = sched.starts_at ? new Date(sched.starts_at) : null;
      const end   = sched.ends_at   ? new Date(sched.ends_at)   : null;

      if (start && now < start) {
        return {
          open: false, label: 'Not started yet',
          reason: 'Opens ' + fmtDt(start, tz),
          nextChange: start
        };
      }
      if (end && now > end) {
        return {
          open: false, label: 'Voting closed',
          reason: 'Closed ' + fmtDt(end, tz),
          nextChange: null
        };
      }
      return {
        open: true, label: 'Voting open',
        reason: end ? 'Closes ' + fmtDt(end, tz) : '',
        nextChange: end
      };
    }

    if (sched.schedule_type === 'recurring') {
      const j          = sched.schedule_json || {};
      const days       = j.days       || [];
      const startTime  = j.start_time || '09:00';
      const endTime    = j.end_time   || '17:00';

      // Get current time in the schedule's timezone
      const nowInTZ   = new Date(now.toLocaleString('en-US', { timeZone: tz }));
      const todayDay  = nowInTZ.getDay();
      const nowMins   = nowInTZ.getHours() * 60 + nowInTZ.getMinutes();
      const startMins = timeToMins(startTime);
      const endMins   = timeToMins(endTime);
      const todayOpen = days.includes(todayDay) && nowMins >= startMins && nowMins < endMins;

      const dayNames  = days.map(d => ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'][d]).join(', ');
      const startFmt  = fmt12(startTime);
      const endFmt    = fmt12(endTime);

      if (todayOpen) {
        // Close time today in UTC
        const closeUTC = closeTimeUTC(now, endTime, tz);
        return {
          open: true, label: 'Voting open',
          reason: `Open today until ${endFmt} (${tz})`,
          nextChange: closeUTC
        };
      }

      const next = nextOpenSlot(days, startTime, tz, now);
      return {
        open: false, label: 'Outside voting hours',
        reason: days.length
          ? `Voting: ${dayNames} · ${startFmt} – ${endFmt} (${tz})`
          : 'No days configured',
        nextChange: next
      };
    }

    return { open: true, label: 'Always open', reason: '', nextChange: null };
  }

  function closeTimeUTC(now, endTime24, tz) {
    const [h, m] = endTime24.split(':').map(Number);
    // Build today's close time in the target timezone
    const tzNow   = new Date(now.toLocaleString('en-US', { timeZone: tz }));
    const dateStr = `${tzNow.getFullYear()}-${pad(tzNow.getMonth()+1)}-${pad(tzNow.getDate())}`;
    return new Date(naiveLocalToUTC(dateStr, endTime24, tz));
  }

  function nextOpenSlot(days, startTime, tz, from) {
    if (!days.length) return null;
    const d = new Date(from);
    for (let i = 1; i <= 7; i++) {
      d.setDate(d.getDate() + 1);
      const dayInTZ = new Date(d.toLocaleString('en-US', { timeZone: tz })).getDay();
      if (days.includes(dayInTZ)) {
        const tzD     = new Date(d.toLocaleString('en-US', { timeZone: tz }));
        const dateStr = `${tzD.getFullYear()}-${pad(tzD.getMonth()+1)}-${pad(tzD.getDate())}`;
        return new Date(naiveLocalToUTC(dateStr, startTime, tz));
      }
    }
    return null;
  }

  /* ── Render status banner ───────────────────────────── */
  function renderStatusBanner(sched, el) {
    if (!el) return;
    const s   = getStatus(sched);
    const cls = s.open ? 'open' : (s.nextChange ? 'upcoming' : 'closed');
    el.innerHTML = `
      <div class="sched-status-banner ${cls}">
        <div class="sched-status-dot"></div>
        <div class="sched-status-text">
          <strong>${s.label}</strong>
          <span>${s.reason}</span>
        </div>
        ${s.nextChange
          ? `<div class="sched-countdown" id="schedCountdown_${el.id||'x'}"></div>`
          : ''}
      </div>`;
    if (s.nextChange) {
      startCountdown(s.nextChange,
        el.querySelector('[id^="schedCountdown"]'));
    }
  }

  /* ── Countdown ──────────────────────────────────────── */
  function startCountdown(target, el) {
    if (!el || !target) return;
    function tick() {
      const diff = new Date(target) - new Date();
      if (diff <= 0) { el.textContent = 'Opening now…'; return; }
      const h = Math.floor(diff / 3600000);
      const m = Math.floor((diff % 3600000) / 60000);
      const s = Math.floor((diff % 60000) / 1000);
      el.textContent = h > 0
        ? `${h}h ${pad(m)}m ${pad(s)}s`
        : `${pad(m)}m ${pad(s)}s`;
      setTimeout(tick, 1000);
    }
    tick();
  }

  /* ── Small helpers ──────────────────────────────────── */
  function timeToMins(t) {
    const [h, m] = t.split(':').map(Number);
    return h * 60 + m;
  }
  function fmt12(time24) {
    const { hour12, minute, period } = time24to12(time24);
    return `${hour12}:${minute} ${period}`;
  }
  function pad(n) { return String(n).padStart(2,'0'); }

  return {
    buildWidget, getValue, setValue, validate,
    getStatus, renderStatusBanner, startCountdown,
    _setType, _toggleDay
  };
})();
