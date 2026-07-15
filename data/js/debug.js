const logContainer = document.getElementById('log-container');

function fetchLogs() {
    fetch('/api/logs')
        .then(response => response.json())
        .then(data => {
            logContainer.innerHTML = '';
            if (data.length === 0) {
                logContainer.innerHTML = '<p style="color:#666;">Noch keine Log-Eintraege vorhanden.</p>';
            } else {
                // Display logs in reverse order (newest first)
                data.slice().reverse().forEach(log => {
                    const div = document.createElement('div');
                    div.textContent = log;
                    logContainer.appendChild(div);
                });
            }
        })
        .catch(err => console.error('Error fetching logs:', err));
}

// Fetch logs every 2 seconds
setInterval(fetchLogs, 2000);
// Initial fetch
fetchLogs();
