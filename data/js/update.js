const dropZone   = document.getElementById('drop-zone');
const fileInput  = document.getElementById('file-input');
const submitBtn  = document.getElementById('submit-btn');
const dropLabel  = document.getElementById('drop-label');
const dropIcon   = document.getElementById('drop-icon');
const modeHint   = document.getElementById('mode-hint');

let selectedFile = null;
let uploadMode   = 'fw'; // 'fw' = Firmware (/update), 'fs' = Dateisystem (/update-fs)

// --- Modus wechseln (Firmware / Dateisystem) ---
function setMode(mode) {
    uploadMode = mode;
    document.getElementById('tab-fw').classList.toggle('active', mode === 'fw');
    document.getElementById('tab-fs').classList.toggle('active', mode === 'fs');

    if (mode === 'fw') {
        modeHint.innerHTML = 'Flasht die Firmware (<code class="text-warning">firmware.bin</code>). '
            + 'PlatformIO-Target: <code>upload</code>';
    } else {
        modeHint.innerHTML = 'Flasht das Dateisystem (<code class="text-warning">littlefs.bin</code>). '
            + 'PlatformIO-Target: <code>buildfs</code> → <code>.pio/build/&lt;env&gt;/littlefs.bin</code>';
    }

    // Dateiauswahl zurücksetzen, damit nicht versehentlich die falsche Datei geflasht wird
    resetDropZone();
}

// --- Drop-Zone zurücksetzen ---
function resetDropZone() {
    selectedFile = null;
    submitBtn.disabled = true;
    dropZone.classList.remove('file-ready', 'drag-over');
    dropZone.style.pointerEvents = '';
    dropIcon.className = 'bi bi-cloud-arrow-up fs-1 text-secondary';
    dropLabel.innerHTML = 'Datei hier ablegen oder <span class="text-warning">klicken</span>';
    document.getElementById('progress-container').classList.add('d-none');
    const bar = document.getElementById('progress-bar');
    bar.style.width = '0%'; bar.innerHTML = '0%';
}

// --- Datei übernehmen und UI aktualisieren ---
function setFile(file) {
    if (!file || !file.name.endsWith('.bin')) {
        alert('Bitte nur .bin-Dateien verwenden!');
        return;
    }
    selectedFile = file;
    dropZone.classList.remove('drag-over');
    dropZone.classList.add('file-ready');
    dropIcon.className = 'bi bi-file-earmark-check fs-1 text-success';
    dropLabel.innerHTML = '<strong class="text-success">' + file.name + '</strong>'
        + ' <span class="text-muted">(' + (file.size / 1024).toFixed(1) + ' KB)</span>';
    submitBtn.disabled = false;
}

// --- Drag & Drop Events ---
dropZone.addEventListener('dragover', e => {
    e.preventDefault();
    dropZone.classList.add('drag-over');
});

dropZone.addEventListener('dragleave', () => {
    dropZone.classList.remove('drag-over');
});

dropZone.addEventListener('drop', e => {
    e.preventDefault();
    setFile(e.dataTransfer.files[0]);
});

// --- Klick → nativen Datei-Dialog öffnen ---
fileInput.addEventListener('change', () => {
    if (fileInput.files.length > 0) setFile(fileInput.files[0]);
});

// --- Upload starten ---
submitBtn.addEventListener('click', () => {
    if (!selectedFile) return;

    // Sicherheitswarnung beim Dateisystem-Flash, da alle Dateien überschrieben werden
    if (uploadMode === 'fs') {
        if (!confirm('Achtung: Das Dateisystem wird komplett überschrieben.\nAlle Änderungen an HTML/JS/CSS/config.json gehen verloren!\n\nFortfahren?')) return;
    }

    document.getElementById('progress-container').classList.remove('d-none');
    submitBtn.disabled = true;
    dropZone.style.pointerEvents = 'none';

    const endpoint = uploadMode === 'fs' ? '/update-fs' : '/update';
    const formData = new FormData();
    formData.append('update', selectedFile);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', endpoint, true);

    // Fortschrittsanzeige während des Uploads
    xhr.upload.addEventListener('progress', evt => {
        if (evt.lengthComputable) {
            const per = Math.round((evt.loaded / evt.total) * 100);
            const bar = document.getElementById('progress-bar');
            bar.style.width = per + '%';
            bar.innerHTML   = per + '%';
        }
    });

    xhr.onload = function () {
        if (xhr.status === 200) {
            alert(xhr.responseText);
            setTimeout(() => { window.location.href = '/'; }, 3000);
        } else {
            alert('Fehler während des Uploads!');
            location.reload();
        }
    };

    xhr.send(formData);
});
