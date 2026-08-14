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
};

const ICON_START = '<path d="M13 2 3 14h7l-1 8 10-12h-7l1-8Z" fill="currentColor"/>';
const ICON_STOP = '<rect x="5" y="5" width="14" height="14" rx="2" fill="currentColor"/>';

let isCharging = false;

function setToggleButton(charging) {
  isCharging = charging;
  els.toggleBtn.classList.toggle('btn-primary', !charging);
  els.toggleBtn.classList.toggle('btn-danger', charging);
  els.toggleIcon.innerHTML = charging ? ICON_STOP : ICON_START;
  els.toggleText.textContent = charging ? 'Stop charging' : 'Start charging';
}

function updateSliderFill() {
  const min = Number(els.amps.min);
  const max = Number(els.amps.max);
  const pct = ((els.amps.value - min) / (max - min)) * 100;
  els.amps.style.setProperty('--fill', pct + '%');
}

function setBadge(mode, text) {
  els.state.classList.remove('idle', 'charging', 'offline');
  els.state.classList.add(mode);
  els.stateText.textContent = text;
}

async function updateStatus() {
  try {
    const res = await fetch('/status', { cache: 'no-store' });
    if (!res.ok) throw new Error('bad response');
    const data = await res.json();

    if (data.online === false) {
      setBadge('offline', 'No connection');
      els.currentAmp.textContent = '--';
      els.power.textContent = '--';
      els.energySession.textContent = '--';
    } else {
      setBadge(data.charging ? 'charging' : 'idle', data.charging ? 'Charging' : 'Ready / Idle');
      els.currentAmp.textContent = data.current;
      els.power.textContent = (data.power / 1000).toFixed(1);
      els.energySession.textContent = (data.energySincePowerOn / 1000).toFixed(2);
    }
    setToggleButton(data.charging === true);
    els.energyTotal.textContent = (data.energyTotal / 1000).toFixed(2);
  } catch (e) {
    setBadge('offline', 'No connection');
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

updateSliderFill();
updateStatus();
setInterval(updateStatus, 3000);
