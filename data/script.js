const els = {
  state: document.getElementById('chargingState'),
  stateText: document.querySelector('#chargingState .badge-text'),
  currentAmp: document.getElementById('currentAmp'),
  power: document.getElementById('power'),
  energySession: document.getElementById('energySession'),
  energyTotal: document.getElementById('energyTotal'),
  amps: document.getElementById('amps'),
  ampsValue: document.getElementById('ampsValue'),
  toggleBtn: document.getElementById('toggleBtn'),
  toggleIcon: document.getElementById('toggleIcon'),
  toggleText: document.getElementById('toggleText'),
  langToggle: document.getElementById('langToggle'),
};

const ICON_START = '<path d="M13 2 3 14h7l-1 8 10-12h-7l1-8Z" fill="currentColor"/>';
const ICON_STOP = '<rect x="5" y="5" width="14" height="14" rx="2" fill="currentColor"/>';

const translations = {
  en: {
    title: 'Wallbox Control',
    loading: 'Loading…',
    offline: 'No connection',
    charging: 'Charging',
    idle: 'Ready / Idle',
    statCurrent: 'Current',
    statPower: 'Power',
    statSessionEnergy: 'Session energy',
    statTotalEnergy: 'Total energy',
    sliderLabel: 'Max. charging current',
    startCharging: 'Start charging',
    stopCharging: 'Stop charging',
  },
  de: {
    title: 'Wallbox-Steuerung',
    loading: 'Lädt…',
    offline: 'Keine Verbindung',
    charging: 'Lädt',
    idle: 'Bereit / Leerlauf',
    statCurrent: 'Strom',
    statPower: 'Leistung',
    statSessionEnergy: 'Sitzungsenergie',
    statTotalEnergy: 'Gesamtenergie',
    sliderLabel: 'Max. Ladestrom',
    startCharging: 'Laden starten',
    stopCharging: 'Laden stoppen',
  },
};

const LANG_STORAGE_KEY = 'wallbox-lang';

function detectDefaultLang() {
  return navigator.language && navigator.language.toLowerCase().startsWith('de') ? 'de' : 'en';
}

let lang = localStorage.getItem(LANG_STORAGE_KEY) || detectDefaultLang();
let isCharging = false;
let lastBadgeMode = null;

function t(key) {
  return translations[lang][key];
}

function applyStaticTranslations() {
  document.documentElement.lang = lang;
  document.title = t('title');
  document.querySelectorAll('[data-i18n]').forEach((el) => {
    el.textContent = t(el.dataset.i18n);
  });
  els.langToggle.textContent = lang === 'en' ? 'DE' : 'EN';
}

function setLang(newLang) {
  lang = newLang;
  localStorage.setItem(LANG_STORAGE_KEY, lang);
  applyStaticTranslations();
  if (lastBadgeMode) setBadge(lastBadgeMode);
  setToggleButton(isCharging);
}

function setToggleButton(charging) {
  isCharging = charging;
  els.toggleBtn.classList.toggle('btn-primary', !charging);
  els.toggleBtn.classList.toggle('btn-danger', charging);
  els.toggleIcon.innerHTML = charging ? ICON_STOP : ICON_START;
  els.toggleText.textContent = t(charging ? 'stopCharging' : 'startCharging');
}

function updateSliderFill() {
  const min = Number(els.amps.min);
  const max = Number(els.amps.max);
  const pct = ((els.amps.value - min) / (max - min)) * 100;
  els.amps.style.setProperty('--fill', pct + '%');
}

function setBadge(mode) {
  lastBadgeMode = mode;
  els.state.classList.remove('idle', 'charging', 'offline');
  els.state.classList.add(mode);
  els.stateText.textContent = t(mode);
}

async function updateStatus() {
  try {
    const res = await fetch('/status', { cache: 'no-store' });
    if (!res.ok) throw new Error('bad response');
    const data = await res.json();

    if (data.online === false) {
      setBadge('offline');
      els.currentAmp.textContent = '--';
      els.power.textContent = '--';
      els.energySession.textContent = '--';
    } else {
      setBadge(data.charging ? 'charging' : 'idle');
      els.currentAmp.textContent = data.current;
      els.power.textContent = (data.power / 1000).toFixed(1);
      els.energySession.textContent = (data.energySincePowerOn / 1000).toFixed(2);
    }
    setToggleButton(data.charging === true);
    els.energyTotal.textContent = (data.energyTotal / 1000).toFixed(2);
  } catch (e) {
    setBadge('offline');
  }
}

async function setCurrent() {
  await fetch('/setcurrent?amps=' + els.amps.value, { cache: 'no-store' });
  await updateStatus();
}

async function stopCharging() {
  await fetch('/stop', { cache: 'no-store' });
  await updateStatus();
}

async function toggleCharging() {
  els.toggleBtn.disabled = true;
  try {
    if (isCharging) {
      await stopCharging();
    } else {
      await setCurrent();
    }
  } finally {
    els.toggleBtn.disabled = false;
  }
}

els.amps.addEventListener('input', () => {
  els.ampsValue.textContent = els.amps.value;
  updateSliderFill();
});

els.toggleBtn.addEventListener('click', toggleCharging);
els.langToggle.addEventListener('click', () => setLang(lang === 'en' ? 'de' : 'en'));

applyStaticTranslations();
updateSliderFill();
updateStatus();
setInterval(updateStatus, 3000);
