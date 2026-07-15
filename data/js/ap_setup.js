const scanBtn = document.getElementById('scanBtn');
const connectBtn = document.getElementById('connectBtn');
const ssidSelect = document.getElementById('ssidSelect');
const passInput = document.getElementById('passInput');
const statusDiv = document.getElementById('status');

let foundSSIDs = [];

scanBtn.onclick = async () => {
    statusDiv.innerText = "Scanne Netzwerke...";
    scanBtn.disabled = true;
    ssidSelect.innerHTML = '<option value="">Scannung läuft...</option>';

    try {
        const response = await fetch('/api/scan');
        const data = await response.json();
        
        ssidSelect.innerHTML = '';
        if (data.length === 0) {
            ssidSelect.innerHTML = '<option value="">Keine Netze gefunden</option>';
        } else {
            data.forEach((wifi, index) => {
                const option = document.createElement('option');
                option.value = wifi.ssid;
                option.textContent = `${wifi.ssid} (${wifi.rssi} dBm)`;
                ssidSelect.appendChild(option);
            });
        }
        statusDiv.innerText = `${data.length} Netzwerke gefunden.`;
    } catch (err) {
        statusDiv.innerText = "Fehler beim Scannen.";
        console.error(err);
    } finally {
        scanBtn.disabled = false;
    }
};

ssidSelect.onchange = () => {
    connectBtn.disabled = !ssidSelect.value;
};

connectBtn.onclick = async () => {
    const ssid = ssidSelect.value;
    const pass = passInput.value;

    if (!ssid || !pass) {
        statusDiv.innerText = "Bitte SSID und Passwort wählen!";
        return;
    }

    statusDiv.innerText = "Verbindung wird aufgebaut...";
    connectBtn.disabled = true;

    try {
        const formData = new URLSearchParams();
        formData.append('ssid', ssid);
        formData.append('pass', pass);

        const response = await fetch('/api/connect', {
            method: 'POST',
            body: formData,
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            }
        });

        const resultText = await response.text();
        if (response.ok) {
            statusDiv.innerText = resultText;
            setTimeout(() => {
                location.reload();
            }, 2000);
        } else {
            statusDiv.innerText = "Fehler: " + resultText;
            connectBtn.disabled = false;
        }
    } catch (err) {
        statusDiv.innerText = "Fehler bei der Verbindung.";
        console.error(err);
        connectBtn.disabled = false;
    }
};
