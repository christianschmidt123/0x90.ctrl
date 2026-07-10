let gateway = `ws://${window.location.hostname}/ws`;
let websocket;
let currentLayer = 0;

function initWebSocket() {
    console.log('Versuche WebSocket-Verbindung...');
    websocket = new WebSocket(gateway);
    websocket.onopen  = () => console.log('WebSocket geöffnet');
    websocket.onclose = () => setTimeout(initWebSocket, 2000);
    websocket.onmessage = (event) => {
        if (event.data.startsWith("LAYER_CHANGED:")) {
            currentLayer = parseInt(event.data.substring(14));
            updateLayerUI();
        } else if (event.data.startsWith("ACK:")) {
            // Bestätigung vom ESP32: Button visuell aufleuchten lassen
            flashButtonUI(event.data.substring(4));
        }
    };
}

// Lässt den Button im Browser visuell aufleuchten
function flashButtonUI(macroId) {
    const btn = document.getElementById("btn_" + macroId);
    if (btn) {
        btn.classList.add("active-press");
        setTimeout(() => btn.classList.remove("active-press"), 150);
    }
}

function triggerMacro(id) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("TRIGGER:" + id);
    }
}

function setLayer(layerId) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send("SET_LAYER:" + layerId);
    }
}

function updateLayerUI() {
    document.querySelectorAll('#layer-container button').forEach((btn, idx) => {
        btn.classList.toggle('active', idx === currentLayer);
    });
    loadMacros();
}

function loadMacros() {
    fetch('/api/macros')
        .then(r => r.json())
        .then(data => {
            const container = document.getElementById('deck-container');
            container.innerHTML = '';

            // Nur Makros des aktuellen Profils anzeigen
            data.macros.filter(m => m.layer === currentLayer).forEach(macro => {
                const btn = document.createElement('div');
                btn.className = 'macro-btn';
                btn.id = "btn_" + macro.id;
                btn.onclick = () => triggerMacro(macro.id);
                btn.innerHTML = `
                    <i class="bi ${macro.icon || 'bi-bookmark'} macro-icon"></i>
                    <div class="macro-name">${macro.name}</div>
                `;
                container.appendChild(btn);
            });
        })
        .catch(err => console.error("Fehler beim Laden der Makros:", err));
}

window.addEventListener('load', () => {
    initWebSocket();
    loadMacros();
});
