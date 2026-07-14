var macrosData = [];
var activeMacroIndexForIcon = null;

const availableIcons = [
    'bi-volume-mute-fill', 'bi-volume-down-fill', 'bi-volume-up-fill',
    'bi-play-fill', 'bi-pause-fill', 'bi-skip-forward-fill', 'bi-skip-backward-fill',
    'bi-spotify', 'bi-youtube', 'bi-discord', 'bi-twitch', 'bi-steam',
    'bi-browser-chrome', 'bi-browser-edge', 'bi-browser-safari', 'bi-globe',
    'bi-gear-fill', 'bi-envelope-fill', 'bi-chat-dots-fill', 'bi-camera-video-fill',
    'bi-mic-fill', 'bi-mic-mute-fill', 'bi-code-slash', 'bi-terminal-fill',
    'bi-folder-fill', 'bi-file-earmark-code', 'bi-copy', 'bi-scissors',
    'bi-clipboard-fill', 'bi-floppy-fill', 'bi-arrow-repeat', 'bi-house-fill',
    'bi-brightness-high-fill', 'bi-moon-fill', 'bi-calculator', 'bi-controller'
];

const allKeys = [
    'CTRL', 'SHIFT', 'ALT',
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    '0','1','2','3','4','5','6','7','8','9',
    'ENTER','SPACE','BACKSPACE','ESC','DELETE','INSERT','HOME','END','PAGE_UP','PAGE_DOWN',
    'UP','DOWN','LEFT','RIGHT',
    'F1','F2','F3','F4','F5','F6','F7','F8','F9','F10','F11','F12'
];

let iconModal;
window.addEventListener('DOMContentLoaded', () => {
    iconModal = new bootstrap.Modal(document.getElementById('iconModal'));
});

// Konfiguration laden und UI aufbauen
fetch('/api/macros?t=' + Date.now())
    .then(r => r.json())
    .then(d => {
        macrosData = d.macros;
        renderMacros();
        buildIconGrid();
    });

function handleTypeChange(index, newType) {
    macrosData[index].type = parseInt(newType);
    // Standardwert je nach Typ setzen
    macrosData[index].keys = (macrosData[index].type === 1) ? "MUTE" : "";
    renderMacros();
}

// Hilfsfunktion: "G+P+I+O+8" → "GPIO8" (für das Eingabefeld)
function keysToPlainText(keyStr) {
    if (!keyStr) return "";
    return keyStr.split('+').join('');
}

// Hilfsfunktion: "GPIO8" → "G+P+I+O+8" (für das JSON)
function plainTextToKeys(text) {
    return text.split('').join('+');
}

function renderMacros() {
    let h = '';
    macrosData.forEach((m, i) => {
        let keyArray = m.keys ? m.keys.split('+').filter(k => k.length > 0) : [];

        h += "<div class='card bg-secondary text-white mb-3 p-3'><div class='row g-2 align-items-center'>";

        // Spalte 1: Name
        h += "<div class='col-md-2'><label class='small text-light-50 d-block'>Name</label>" +
             "<input type='text' class='form-control form-control-sm bg-dark text-white border-0' " +
             "value='" + m.name + "' oninput='macrosData[" + i + "].name=this.value'></div>";

        // Spalte 2: Typ
        h += "<div class='col-md-2'><label class='small text-light-50 d-block'>Typ</label>" +
             "<select class='form-select form-select-sm bg-dark text-white border-0' onchange='handleTypeChange(" + i + ", this.value)'>" +
             "<option value='0' " + (m.type==0?"selected":"") + ">Hotkey (Gleichzeitig)</option>" +
             "<option value='2' " + (m.type==2?"selected":"") + ">Text (Nacheinander)</option>" +
             "<option value='1' " + (m.type==1?"selected":"") + ">Media (Audio)</option>" +
             "</select></div>";

        // Spalte 3: Dynamisches Eingabefeld je nach Typ
        h += "<div class='col-md-4'><label class='small text-light-50 d-block'>Tastatur-Kombination / Text</label>";

        if (m.type == 1) {
            // MEDIA-MODUS: Dropdown für Medientasten
            h += "<select class='form-select form-select-sm bg-dark text-white border-0' onchange='macrosData[" + i + "].keys=this.value'>" +
                 "<option value='MUTE' "     + (m.keys=='MUTE'    ?"selected":"") + ">Stummschalten (Mute)</option>" +
                 "<option value='VOL_UP' "   + (m.keys=='VOL_UP'  ?"selected":"") + ">Lauter (Vol +)</option>" +
                 "<option value='VOL_DOWN' " + (m.keys=='VOL_DOWN'?"selected":"") + ">Leiser (Vol -)</option>" +
                 "</select>";
        } else if (m.type == 2) {
            // TEXT-MODUS: Freitextfeld
            let currentText = keysToPlainText(m.keys);
            h += "<input type='text' class='form-control form-control-sm bg-dark text-white border-0' " +
                 "placeholder='Freitext hier eintippen...' value='" + currentText + "' " +
                 "oninput='macrosData[" + i + "].keys=plainTextToKeys(this.value)'>";
        } else {
            // HOTKEY-MODUS: Badges für Tastenkombinationen
            h += "<div class='d-flex flex-wrap gap-1 align-items-center bg-dark p-2 rounded' style='min-height:38px;'>";
            keyArray.forEach((key, kIdx) => {
                h += "<span class='badge bg-info d-flex align-items-center gap-1'>" + key +
                     "<i class='bi bi-x-circle-fill text-danger' style='cursor:pointer;' " +
                     "onclick='removeKey(" + i + "," + kIdx + ")'></i></span>";
            });
            h += "<select class='form-select form-select-sm bg-secondary text-white border-0 py-0 px-1' " +
                 "style='width:auto; max-width:100px;' onchange='addKey(" + i + ", this.value); this.value=\"\";'>" +
                 "<option value=''>+ Taste</option>";
            allKeys.forEach(k => { h += "<option value='" + k + "'>" + k + "</option>"; });
            h += "</select></div>";
        }
        h += "</div>";

        // Spalte 4: GPIO
        h += "<div class='col-md-1'><label class='small text-light-50 d-block'>GPIO</label>" +
             "<input type='number' class='form-control form-control-sm bg-dark text-white border-0' " +
             "value='" + m.gpio + "' oninput='macrosData[" + i + "].gpio=parseInt(this.value)'></div>";

        // Spalte 5: Icon-Auswahl
        h += "<div class='col-md-3'><label class='small text-light-50 d-block'>Icon</label>" +
             "<button class='btn btn-sm btn-dark w-100 text-start border-0 d-flex align-items-center justify-content-between py-2' " +
             "onclick='openIconPicker(" + i + ")'>" +
             "<span><i class='" + (m.icon || 'bi-question-lg') + " me-2 fs-5' id='btn-icon-" + i + "'></i> " +
             "<span class='small text-muted'>" + (m.icon || 'Keins') + "</span></span>" +
             "<i class='bi bi-chevron-down small text-muted'></i></button></div>";

        h += "</div></div>";
    });
    document.getElementById('macro-list').innerHTML = h;
}

function buildIconGrid() {
    let gridHtml = '';
    availableIcons.forEach(icon => {
        gridHtml += `<div class="col">
            <div class="bg-secondary p-3 rounded icon-grid-item text-center fs-3" onclick="selectIcon('${icon}')" title="${icon}">
                <i class="${icon}"></i>
            </div>
        </div>`;
    });
    document.getElementById('icon-picker-grid').innerHTML = gridHtml;
}

function openIconPicker(index) {
    activeMacroIndexForIcon = index;
    if (iconModal) iconModal.show();
}

function selectIcon(iconClass) {
    if (activeMacroIndexForIcon !== null) {
        macrosData[activeMacroIndexForIcon].icon = iconClass;
        renderMacros();
    }
    if (iconModal) iconModal.hide();
}

function addKey(macroIdx, key) {
    if (!key) return;
    let currentKeys = macrosData[macroIdx].keys
        ? macrosData[macroIdx].keys.split('+').filter(k => k.length > 0)
        : [];
    currentKeys.push(key);
    macrosData[macroIdx].keys = currentKeys.join('+');
    renderMacros();
}

function removeKey(macroIdx, keyIdx) {
    let currentKeys = macrosData[macroIdx].keys.split('+').filter(k => k.length > 0);
    currentKeys.splice(keyIdx, 1);
    macrosData[macroIdx].keys = currentKeys.join('+');
    renderMacros();
}

function saveConfig() {
    const payload = JSON.stringify({ macros: macrosData }, null, 2);
    fetch('/api/macros/save', {
        method: 'POST',
        headers: {
            'Content-Type': 'text/plain',
            'X-Filename': 'config.json'
        },
        body: payload
    }).then(r => {
        if (r.ok) { alert('Erfolgreich gespeichert!'); location.reload(); }
        else       { alert('Fehler beim Speichern'); }
    });
}
