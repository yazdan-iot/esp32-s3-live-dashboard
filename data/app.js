/**
 * @file  app.js
 * @brief Dashboard client logic: WebSocket connection, metrics rendering,
 *        LED control.
 */

const Dashboard = {
  ws: null,
  reconnectDelayMs: 1000,
  isUserDraggingSlider: false,

  // --------------------------------------------------------------------------
  // Init
  // --------------------------------------------------------------------------
  init() {
    this.cacheDomRefs();
    this.bindControlEvents();
    this.fetchInitialSnapshot();   // fills the page before the first WS message
    this.connectWebSocket();
  },

  /// Caches DOM references once at startup to avoid repeated querySelector
  /// calls on every metrics update.
  cacheDomRefs() {
    this.dom = {
      connDot:      document.getElementById('conn-dot'),
      connStatus:   document.getElementById('conn-status'),
      footerStatus: document.getElementById('footer-status'),

      ledOrbWrap:     document.querySelector('.led-orb-wrap'),
      ledOrb:         document.getElementById('led-orb'),
      ledGlow:        document.getElementById('led-glow'),
      colorPicker:    document.getElementById('color-picker'),
      brightnessSlider: document.getElementById('brightness-slider'),
      brightnessValue:  document.getElementById('brightness-value'),
      modeButtons:    document.querySelectorAll('.mode-btn'),
      presetButtons:  document.querySelectorAll('.preset-swatch'),

      signalBars: document.querySelectorAll('#signal-bars span'),
      rssiDbm:    document.getElementById('rssi-dbm'),
    };
  },

  // --------------------------------------------------------------------------
  // WebSocket
  // --------------------------------------------------------------------------

  connectWebSocket() {
    // Protocol/host derived from the current page URL, so this works for
    // both a direct IP and esp32-dashboard.local without changes.
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    this.ws = new WebSocket(`${protocol}//${location.host}/ws`);

    this.ws.onopen = () => {
      this.setConnectionState(true);
      this.reconnectDelayMs = 1000;
    };

    this.ws.onclose = () => {
      this.setConnectionState(false);
      this.scheduleReconnect();
    };

    this.ws.onerror = () => {
      this.ws.close();  // triggers onclose, which starts the reconnect cycle
    };

    this.ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      if (data.metrics) this.renderMetrics(data.metrics);
      if (data.led)     this.renderLedState(data.led);
    };
  },

  /// Reconnects with exponential backoff (capped at 10s).
  scheduleReconnect() {
    setTimeout(() => this.connectWebSocket(), this.reconnectDelayMs);
    this.reconnectDelayMs = Math.min(this.reconnectDelayMs * 1.5, 10000);
  },

  setConnectionState(isConnected) {
    this.dom.connDot.classList.toggle('is-live', isConnected);
    this.dom.connStatus.textContent = isConnected ? 'Connected · live' : 'Disconnected -- retrying…';
  },

  /// One-off HTTP fetch so the page has real values immediately on load,
  /// instead of showing "--" until the first WebSocket tick.
  async fetchInitialSnapshot() {
    try {
      const response = await fetch('/api/status');
      const data = await response.json();
      this.renderMetrics(data.metrics);
      this.renderLedState(data.led);
    } catch (err) {
      console.error('[Dashboard] Failed to fetch initial snapshot:', err);
    }
  },

  // --------------------------------------------------------------------------
  // LED control commands
  // --------------------------------------------------------------------------

  /// Sends a JSON command over WebSocket; silently drops it if not
  /// connected yet (no point queuing stale commands).
  sendCommand(payload) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(payload));
    }
  },

  bindControlEvents() {
    this.dom.colorPicker.addEventListener('input', (e) => {
      const { r, g, b } = this.hexToRgb(e.target.value);
      this.applyColorToOrb(e.target.value);
      this.sendCommand({ action: 'setColor', r, g, b });
    });

    this.dom.brightnessSlider.addEventListener('input', (e) => {
      const value = Number(e.target.value);
      this.dom.brightnessValue.textContent = value;
      this.sendCommand({ action: 'setBrightness', value });
    });

    this.dom.modeButtons.forEach((btn) => {
      btn.addEventListener('click', () => {
        this.setActiveModeButton(btn.dataset.mode);
        this.sendCommand({ action: 'setMode', mode: btn.dataset.mode });
      });
    });

    this.dom.presetButtons.forEach((btn) => {
      btn.addEventListener('click', () => {
        const [r, g, b] = btn.dataset.rgb.split(',').map(Number);
        const hex = this.rgbToHex(r, g, b);
        this.dom.colorPicker.value = hex;
        this.applyColorToOrb(hex);
        this.sendCommand({ action: 'setColor', r, g, b });
      });
    });
  },

  // --------------------------------------------------------------------------
  // LED state rendering
  // --------------------------------------------------------------------------

  renderLedState(led) {
    const hex = this.rgbToHex(led.r, led.g, led.b);
    this.dom.colorPicker.value = hex;
    this.dom.brightnessSlider.value = led.brightness;
    this.dom.brightnessValue.textContent = led.brightness;
    this.applyColorToOrb(hex);
    this.setActiveModeButton(led.mode);
  },

  /// Sets the LED orb color via a CSS custom property, so the browser
  /// animates the CSS transition instead of us doing it manually.
  applyColorToOrb(hexColor) {
    this.dom.ledOrbWrap.style.setProperty('--led-color', hexColor);
  },

  setActiveModeButton(mode) {
    this.dom.modeButtons.forEach((btn) => {
      btn.classList.toggle('is-active', btn.dataset.mode === mode);
    });
    this.dom.ledOrbWrap.classList.remove('is-off', 'is-breathe');
    if (mode === 'off')     this.dom.ledOrbWrap.classList.add('is-off');
    if (mode === 'breathe') this.dom.ledOrbWrap.classList.add('is-breathe');
  },

  // --------------------------------------------------------------------------
  // System metrics rendering
  // --------------------------------------------------------------------------

  renderMetrics(m) {
    this.renderNetwork(m.network);
    this.renderMemory(m.memory);
    this.renderStorage(m.storage);
    this.renderChip(m.chip);
    this.renderRuntime(m.runtime);

    this.dom.footerStatus.textContent =
      `Last update: ${new Date().toLocaleTimeString()}`;
  },

  renderNetwork(net) {
    this.setText('m-ssid',    net.ssid || '--');
    this.setText('m-ip',      net.ip || '--');
    this.setText('m-gateway', net.gateway || '--');
    this.setText('m-subnet',  net.subnet || '--');
    this.setText('m-dns',     net.dns || '--');
    this.setText('m-channel', net.channel);
    this.setText('m-mac',     net.mac);

    this.dom.rssiDbm.textContent = `${net.rssiDbm} dBm`;

    // Number of active signal bars scales with signal percent (1 bar = 20%)
    const activeBarCount = Math.ceil(net.rssiPercent / 20);
    this.dom.signalBars.forEach((bar, index) => {
      bar.classList.toggle('is-active', index < activeBarCount);
    });
  },

  renderMemory(mem) {
    this.updateMeter('heap-fill', 'heap-text', mem.totalHeap - mem.freeHeap, mem.totalHeap);
    this.updateMeter('psram-fill', 'psram-text', mem.totalPsram - mem.freePsram, mem.totalPsram);
    this.setText('m-min-heap', this.formatBytes(mem.minFreeHeap));
  },

  renderStorage(storage) {
    this.updateMeter('sketch-fill', 'sketch-text', storage.sketchSize,
                      storage.sketchSize + storage.freeSketchSpace);
    this.updateMeter('fs-fill', 'fs-text', storage.fsUsed, storage.fsTotal);
    this.setText('m-flash-total', this.formatBytes(storage.flashSize));
    this.setText('m-free-sketch', this.formatBytes(storage.freeSketchSpace));
  },

  renderChip(chip) {
    this.setText('m-temp', Number(chip.temperatureC).toFixed(1));
    this.setText('m-chip-model', chip.model);
    this.setText('m-chip-rev', `rev ${chip.revision}`);
    this.setText('m-cpu-freq', `${chip.cpuFreqMhz} MHz`);
    this.setText('m-cores', chip.coreCount);
    this.setText('m-sdk', chip.sdkVersion);
  },

  renderRuntime(runtime) {
    this.setText('m-uptime', this.formatUptime(runtime.uptimeMs));
    this.setText('m-boot-count', runtime.bootCount);
    this.setText('m-http-requests', runtime.httpRequests);
    this.setText('m-ws-clients', runtime.wsClients);
    this.setText('m-reset-reason', runtime.lastResetReason);
  },

  // --------------------------------------------------------------------------
  // Formatting helpers
  // --------------------------------------------------------------------------

  setText(elementId, value) {
    const el = document.getElementById(elementId);
    if (el) el.textContent = value;
  },

  /// Updates a progress bar fill + its label text in one call.
  updateMeter(fillId, textId, usedBytes, totalBytes) {
    const percent = totalBytes > 0 ? (usedBytes / totalBytes) * 100 : 0;
    document.getElementById(fillId).style.width = `${percent.toFixed(1)}%`;
    this.setText(textId, `${this.formatBytes(usedBytes)} / ${this.formatBytes(totalBytes)}`);
  },

  /// Converts raw bytes to a human-readable unit (KB/MB).
  formatBytes(bytes) {
    if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
    if (bytes >= 1024)        return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
  },

  /// Converts uptime in ms to a "Dd HH:MM:SS" readable format.
  formatUptime(ms) {
    const totalSeconds = Math.floor(ms / 1000);
    const days    = Math.floor(totalSeconds / 86400);
    const hours   = Math.floor((totalSeconds % 86400) / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    const pad = (n) => String(n).padStart(2, '0');

    return days > 0
      ? `${days}d ${pad(hours)}:${pad(minutes)}:${pad(seconds)}`
      : `${pad(hours)}:${pad(minutes)}:${pad(seconds)}`;
  },

  hexToRgb(hex) {
    const num = parseInt(hex.slice(1), 16);
    return { r: (num >> 16) & 255, g: (num >> 8) & 255, b: num & 255 };
  },

  rgbToHex(r, g, b) {
    return '#' + [r, g, b].map((c) => c.toString(16).padStart(2, '0')).join('');
  },
};

document.addEventListener('DOMContentLoaded', () => Dashboard.init());
