const form = document.getElementById('upload-form');

form.addEventListener('submit', e => {
    e.preventDefault();
    const fileInput = document.getElementById('file-input');
    if (fileInput.files.length === 0) return;

    document.getElementById('progress-container').classList.remove('d-none');
    document.getElementById('submit-btn').disabled = true;

    const formData = new FormData();
    formData.append("update", fileInput.files[0]);

    const xhr = new XMLHttpRequest();
    xhr.open("POST", "/update", true);

    xhr.upload.addEventListener("progress", evt => {
        if (evt.lengthComputable) {
            const per = Math.round((evt.loaded / evt.total) * 100);
            const bar = document.getElementById('progress-bar');
            bar.style.width = per + "%";
            bar.innerHTML   = per + "%";
        }
    });

    xhr.onload = function () {
        if (xhr.status === 200) {
            alert(xhr.responseText);
            setTimeout(() => { window.location.href = "/"; }, 3000);
        } else {
            alert("Fehler während des Uploads!");
            location.reload();
        }
    };

    xhr.send(formData);
});
