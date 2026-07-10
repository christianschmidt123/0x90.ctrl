var editor = ace.edit("editor");
editor.setTheme("ace/theme/tomorrow_night_eighties");
editor.session.setMode("ace/mode/html");
var activeFile = "";

function loadFileList() {
    // Cache-Buster für die Dateiliste
    fetch('/api/listfiles?t=' + Date.now())
        .then(r => r.json())
        .then(data => {
            const list = document.getElementById('file-list');
            list.innerHTML = '';
            data.files.forEach(f => {
                const div = document.createElement('div');
                div.className = 'file-item p-2 mb-1';
                if (f.name === activeFile) div.className += ' file-active';
                div.innerHTML = `<i class="bi bi-file-earmark-code"></i> ${f.name} <small class="text-muted" style="float:right;">${f.size} B</small>`;
                div.onclick = () => openFile(f.name);
                list.appendChild(div);
            });
        });
}

function openFile(filename) {
    activeFile = filename;
    document.getElementById('current-file-title').innerText = "Editiere: /" + filename;

    // Syntax-Highlighting je nach Dateiendung
    if      (filename.endsWith('.json')) editor.session.setMode("ace/mode/json");
    else if (filename.endsWith('.html')) editor.session.setMode("ace/mode/html");
    else if (filename.endsWith('.css'))  editor.session.setMode("ace/mode/css");
    else if (filename.endsWith('.js'))   editor.session.setMode("ace/mode/javascript");
    else                                 editor.session.setMode("ace/mode/text");

    // Cache-Buster (?t=...) umgeht den Browser-Cache
    let url = (filename === "config.json" || filename === "/config.json")
        ? "/api/macros"
        : "/" + filename;
    url += "?t=" + Date.now();

    fetch(url)
        .then(r => { if (!r.ok) throw new Error(); return r.text(); })
        .then(text => {
            editor.setValue(text, -1);
            // Cursor ganz nach oben setzen und Undo-Verlauf löschen
            editor.session.getUndoManager().reset();
        })
        .catch(() => { editor.setValue("Fehler beim Laden oder Datei ist leer.", -1); });

    // Highlight-Auswahl in der Sidebar aktualisieren
    document.querySelectorAll('.file-item').forEach(el => {
        el.classList.toggle('file-active', el.innerText.includes(filename));
    });
}

function saveFile() {
    if (!activeFile) { alert("Bitte wähle zuerst eine Datei aus!"); return; }
    const content = editor.getValue();

    fetch('/api/macros/save', {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain', 'X-Filename': activeFile },
        body: content
    }).then(r => {
        if (r.ok) {
            alert(activeFile + " erfolgreich gespeichert!");
            // Datei sofort mit neuem Zeitstempel neu laden
            setTimeout(() => {
                openFile(activeFile);
                loadFileList();
            }, 300);
        } else {
            alert("Fehler beim Speichern auf dem ESP32.");
        }
    });
}

window.onload = () => { loadFileList(); };
