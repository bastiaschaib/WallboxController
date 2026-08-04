const els = {
  state: document.getElementById('chargingState'),
  stateText: document.querySelector('#chargingState .badge-text'),
  currentAmp: document.getElementById('currentAmp'),
  power: document.getElementById('power'),
  amps: document.getElementById('amps'),
  ampsValue: document.getElementById('ampsValue'),
  startBtn: document.getElementById('startBtn'),
  stopBtn: document.getElementById('stopBtn'),
};

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

    setBadge(data.charging ? 'charging' : 'idle', data.charging ? 'Charging' : 'Ready / Idle');
    els.currentAmp.textContent = data.current;
    els.power.textContent = data.power;
  } catch (e) {
    setBadge('offline', 'No connection');
  }
}

async function setCurrent() {
  els.startBtn.disabled = true;
  try {
    await fetch('/setcurrent?amps=' + els.amps.value, { cache: 'no-store' });
    await updateStatus();
  } finally {
    els.startBtn.disabled = false;
  }
}

async function stopCharging() {
  els.stopBtn.disabled = true;
  try {
    await fetch('/stop', { cache: 'no-store' });
    await updateStatus();
  } finally {
    els.stopBtn.disabled = false;
  }
}

els.amps.addEventListener('input', () => {
  els.ampsValue.textContent = els.amps.value;
  updateSliderFill();
});

els.startBtn.addEventListener('click', setCurrent);
els.stopBtn.addEventListener('click', stopCharging);

updateSliderFill();
updateStatus();
setInterval(updateStatus, 3000);
