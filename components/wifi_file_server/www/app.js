// Get current path from URL
function getCurrentPath() {
    const params = new URLSearchParams(window.location.search);
    return params.get('path') || '/';
}

// Get current page
function getCurrentPage() {
    const params = new URLSearchParams(window.location.search);
    return parseInt(params.get('page') || '1');
}

// Load file list
async function loadFiles() {
    const path = getCurrentPath();
    const page = getCurrentPage();

    try {
        const response = await fetch(`/api/list?path=${encodeURIComponent(path)}&page=${page}`);
        const data = await response.json();

        // Update path display with action buttons
        document.getElementById('path').innerHTML = `
            <div class="path-nav">
                <div class="path-left">
                    ${path !== '/' ? `<button onclick="goUp()">⬆️ Back</button>` : ''}
                    <span>Path: ${path}</span>
                </div>
                <div class="path-actions">
                    <button onclick="createFolder()">New Folder</button>
                    <span id="selectionInfo" style="display: none; margin-left: 10px; font-weight: bold;"></span>
                    <button id="downloadBtn" onclick="downloadSelected()" style="display: none;">Download</button>
                    <button id="renameBtn" onclick="renameSelected()" style="display: none;">Rename</button>
                    <button id="deleteBtn" onclick="deleteSelected()" style="display: none;">Delete</button>
                    <button id="clearBtn" onclick="clearSelection()" style="display: none;">Clear</button>
                </div>
            </div>
        `;

        // Update file list
        const fileList = document.getElementById('fileList');

        let html = '';

        // Add parent directory link if not at root
        if (path !== '/') {
            html += `<div class="file parent-dir">
                <a href="#" onclick="goUp(); return false;" class="dir">⬆️ ..</a>
            </div>`;
        }

        if (data.files.length === 0) {
            html += '<p>No files</p>';
        } else {
            html += data.files.map(file => {
                if (file.is_dir) {
                    return `<div class="file">
                        <input type="checkbox" class="file-checkbox" value="${file.path}" data-isdir="true" onchange="updateBulkActions()">
                        <a href="#" onclick="navigateToDir('${file.path}'); return false;" class="dir">${file.name}</a>
                    </div>`;
                } else {
                    const sizeKB = (file.size / 1024).toFixed(0);
                    const warning = validateFAT32Filename(file.name);
                    const warningIcon = warning ? ' ⚠️' : '';
                    return `<div class="file">
                        <input type="checkbox" class="file-checkbox" value="${file.path}" data-isdir="false" onchange="updateBulkActions()">
                        <span>${file.name}${warningIcon} (${sizeKB} KB)</span>
                    </div>`;
                }
            }).join('');
        }

        fileList.innerHTML = html;

        // Update pagination
        const pagination = document.getElementById('pagination');
        if (data.total_pages > 1) {
            let html = '<div class="pagination">';

            // Previous button
            if (page > 1) {
                html += `<button onclick="goToPage(${page - 1})">‹ Back</button>`;
            } else {
                html += `<button disabled>‹ Back</button>`;
            }

            // Page numbers
            const maxButtons = 5;
            let startPage = Math.max(1, page - Math.floor(maxButtons / 2));
            let endPage = Math.min(data.total_pages, startPage + maxButtons - 1);

            // Adjust start if we're near the end
            if (endPage - startPage < maxButtons - 1) {
                startPage = Math.max(1, endPage - maxButtons + 1);
            }

            // First page + ellipsis
            if (startPage > 1) {
                html += `<button onclick="goToPage(1)">1</button>`;
                if (startPage > 2) {
                    html += `<span>...</span>`;
                }
            }

            // Page number buttons
            for (let i = startPage; i <= endPage; i++) {
                if (i === page) {
                    html += `<button class="active">${i}</button>`;
                } else {
                    html += `<button onclick="goToPage(${i})">${i}</button>`;
                }
            }

            // Ellipsis + last page
            if (endPage < data.total_pages) {
                if (endPage < data.total_pages - 1) {
                    html += `<span>...</span>`;
                }
                html += `<button onclick="goToPage(${data.total_pages})">${data.total_pages}</button>`;
            }

            // Next button
            if (page < data.total_pages) {
                html += `<button onclick="goToPage(${page + 1})">Next ›</button>`;
            } else {
                html += `<button disabled>Next ›</button>`;
            }

            html += '</div>';
            pagination.innerHTML = html;
        } else {
            pagination.innerHTML = '';
        }

        // Update info
        document.getElementById('info').textContent =
            `${data.total_files} files | IP: ${data.ip}`;

    } catch (error) {
        document.getElementById('fileList').innerHTML = `<p>Error: ${error.message}</p>`;
    }
}

// Navigate to directory
function navigateToDir(path) {
    window.location.href = `?path=${encodeURIComponent(path)}`;
}

// Navigate up one directory
function goUp() {
    const path = getCurrentPath();
    const parts = path.split('/').filter(p => p);
    parts.pop();
    const newPath = '/' + parts.join('/');
    window.location.href = `?path=${encodeURIComponent(newPath)}`;
}

// Go to specific page
function goToPage(page) {
    const path = getCurrentPath();
    window.location.href = `?path=${encodeURIComponent(path)}&page=${page}`;
}

// Download file
function downloadFile(path) {
    window.location.href = `/api/download?path=${encodeURIComponent(path)}`;
}

// Delete file
async function deleteFile(path) {
    if (!confirm('Delete this file?')) return;

    try {
        const response = await fetch(`/api/delete?path=${encodeURIComponent(path)}`, {
            method: 'DELETE'
        });
        if (response.ok) {
            loadFiles();
        } else {
            alert('Delete failed');
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Validate FAT32 filename
function validateFAT32Filename(filename) {
    // FAT32 invalid characters
    const invalidChars = /[\\/:*?"<>|]/;

    if (filename.length > 255) {
        return 'Filename too long (max 255 characters)';
    }

    if (invalidChars.test(filename)) {
        return 'Invalid characters: \\ / : * ? " < > |';
    }

    // Check for reserved names
    const reserved = ['CON', 'PRN', 'AUX', 'NUL', 'COM1', 'COM2', 'COM3', 'COM4',
        'COM5', 'COM6', 'COM7', 'COM8', 'COM9', 'LPT1', 'LPT2',
        'LPT3', 'LPT4', 'LPT5', 'LPT6', 'LPT7', 'LPT8', 'LPT9'];
    const nameWithoutExt = filename.split('.')[0].toUpperCase();
    if (reserved.includes(nameWithoutExt)) {
        return 'Reserved filename';
    }

    return null;
}

// Toggle folder upload mode
function toggleFolderMode() {
    const fileInput = document.getElementById('fileInput');
    const folderMode = document.getElementById('folderMode').checked;

    if (folderMode) {
        fileInput.setAttribute('webkitdirectory', '');
        fileInput.setAttribute('directory', '');
    } else {
        fileInput.removeAttribute('webkitdirectory');
        fileInput.removeAttribute('directory');
    }
    fileInput.value = '';
}

// Upload files or folder
async function uploadFiles() {
    const fileInput = document.getElementById('fileInput');
    const files = Array.from(fileInput.files);
    if (files.length === 0) return;

    const uploadStatus = document.getElementById('uploadStatus');
    const basePath = getCurrentPath();
    const isFolderMode = document.getElementById('folderMode').checked;
    let successCount = 0;
    let failCount = 0;

    for (let i = 0; i < files.length; i++) {
        const file = files[i];
        const displayName = isFolderMode ? (file.webkitRelativePath || file.name) : file.name;

        uploadStatus.innerHTML = `<div class="progress-text">Uploading ${i + 1}/${files.length}: ${displayName}...</div>`;

        let targetPath = basePath;

        // Handle folder structure
        if (isFolderMode && file.webkitRelativePath) {
            const pathParts = file.webkitRelativePath.split('/');
            pathParts.pop();
            const folderPath = pathParts.join('/');

            if (folderPath) {
                targetPath = basePath === '/' ? `/${folderPath}` : `${basePath}/${folderPath}`;
                await createFolderPath(targetPath);
            }
        }

        const result = await uploadSingleFile(file, targetPath);
        if (result) {
            successCount++;
        } else {
            failCount++;
        }
    }

    uploadStatus.innerHTML = `<div class="upload-success">✓ Uploaded ${successCount} file(s)${failCount > 0 ? `, ${failCount} failed` : ''}</div>`;
    setTimeout(() => {
        uploadStatus.innerHTML = '';
        fileInput.value = '';
        loadFiles();
    }, 2000);
}

// Helper: Create folder path recursively
async function createFolderPath(path) {
    const parts = path.split('/').filter(p => p);
    let currentPath = '';

    for (const part of parts) {
        currentPath += '/' + part;
        try {
            await fetch('/api/mkdir', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ path: currentPath })
            });
        } catch (error) {
            // Folder might already exist, continue
        }
    }
}

// Helper: Upload single file
async function uploadSingleFile(file, targetPath) {
    // Check file extension
    const allowedExtensions = ['.bin', '.gb', '.gbc', '.txt', '.json'];
    const fileName = file.name.toLowerCase();
    const hasValidExtension = allowedExtensions.some(ext => fileName.endsWith(ext));

    if (!hasValidExtension) {
        return false;
    }

    // Validate filename for FAT32
    const validationError = validateFAT32Filename(file.name);
    if (validationError) {
        return false;
    }

    const formData = new FormData();
    formData.append('file', file);
    formData.append('path', targetPath);

    try {
        const response = await fetch('/api/upload', {
            method: 'POST',
            body: formData
        });
        return response.ok;
    } catch (error) {
        return false;
    }
}

// Upload file (legacy single file upload)
function uploadFile() {
    const fileInput = document.getElementById('fileInput');
    const file = fileInput.files[0];
    if (!file) return;

    // Check file extension
    const allowedExtensions = ['.bin', '.gb', '.gbc', '.txt', '.json'];
    const fileName = file.name.toLowerCase();
    const hasValidExtension = allowedExtensions.some(ext => fileName.endsWith(ext));

    if (!hasValidExtension) {
        const uploadStatus = document.getElementById('uploadStatus');
        uploadStatus.innerHTML = `<div class="upload-error">✗ Only .bin, .gb, .gbc, .txt, .json files allowed</div>`;
        return;
    }

    // Validate filename for FAT32
    const validationError = validateFAT32Filename(file.name);
    if (validationError) {
        const uploadStatus = document.getElementById('uploadStatus');
        uploadStatus.innerHTML = `<div class="upload-error">✗ ${validationError}</div>`;
        return;
    }

    const path = getCurrentPath();
    const formData = new FormData();
    formData.append('file', file);
    formData.append('path', path);

    // Create progress elements
    const uploadStatus = document.getElementById('uploadStatus');
    uploadStatus.innerHTML = `
        <div class="upload-progress">
            <div class="progress-bar">
                <div class="progress-fill" id="progressFill"></div>
            </div>
            <div class="progress-text" id="progressText">Uploading ${file.name}... 0%</div>
        </div>
    `;

    const xhr = new XMLHttpRequest();

    // Track upload progress
    xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
            const percent = Math.round((e.loaded / e.total) * 100);
            document.getElementById('progressFill').style.width = percent + '%';
            document.getElementById('progressText').textContent =
                `Uploading ${file.name}... ${percent}%`;
        }
    });

    // Handle completion
    xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
            uploadStatus.innerHTML = '<div class="upload-success">✓ Upload complete!</div>';
            setTimeout(() => {
                uploadStatus.innerHTML = '';
                fileInput.value = '';
                loadFiles();
            }, 2000);
        } else {
            uploadStatus.innerHTML = '<div class="upload-error">✗ Upload failed</div>';
        }
    });

    // Handle errors
    xhr.addEventListener('error', () => {
        uploadStatus.innerHTML = '<div class="upload-error">✗ Upload error</div>';
    });

    xhr.open('POST', '/api/upload');
    xhr.send(formData);
}

// Toggle settings panel
function toggleSettings() {
    const panel = document.getElementById('settingsPanel');
    const button = document.getElementById('settingsToggle');
    if (panel.style.display === 'none') {
        panel.style.display = 'block';
        button.textContent = 'Hide';
        loadSettings();
    } else {
        panel.style.display = 'none';
        button.textContent = 'Settings';
    }
}

// Load current settings
async function loadSettings() {
    try {
        console.log('Loading settings...');
        const response = await fetch('/api/settings');
        const data = await response.json();
        console.log('Settings data:', data);
        document.getElementById('ssidInput').value = data.ssid || '';
        document.getElementById('passwordInput').value = data.password || '';
        document.getElementById('hideSSID').checked = data.hide_ssid || false;
    } catch (error) {
        console.error('Failed to load settings:', error);
    }
}

// Save settings
async function saveSettings() {
    const ssid = document.getElementById('ssidInput').value.trim();
    const password = document.getElementById('passwordInput').value;
    const hideSSID = document.getElementById('hideSSID').checked;
    const status = document.getElementById('settingsStatus');

    if (!ssid) {
        status.innerHTML = '<span style="color: #f44336;">SSID required</span>';
        return;
    }

    if (password.length < 8) {
        status.innerHTML = '<span style="color: #f44336;">Password must be at least 8 characters</span>';
        return;
    }

    try {
        status.innerHTML = '<span style="color: #666;">Saving...</span>';

        const response = await fetch('/api/settings', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ssid, password, hide_ssid: hideSSID })
        });

        if (response.ok) {
            status.innerHTML = '<span style="color: #4caf50;">✓ Saved! Device restarting...</span>';
            setTimeout(() => {
                alert('Settings saved. Device is restarting. Please reconnect to the new WiFi network.');
            }, 1000);
        } else {
            status.innerHTML = '<span style="color: #f44336;">✗ Save failed</span>';
        }
    } catch (error) {
        status.innerHTML = '<span style="color: #f44336;">✗ Error: ' + error.message + '</span>';
    }
}

// Create new folder
async function createFolder() {
    const folderName = prompt('Enter folder name:');
    if (!folderName) return;

    const validationError = validateFAT32Filename(folderName);
    if (validationError) {
        alert(validationError);
        return;
    }

    const path = getCurrentPath();
    const folderPath = path === '/' ? `/${folderName}` : `${path}/${folderName}`;

    try {
        const response = await fetch('/api/mkdir', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ path: folderPath })
        });

        if (response.ok) {
            loadFiles();
        } else {
            alert('Failed to create folder');
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Delete folder
async function deleteFolder(path) {
    if (!confirm(`Delete folder "${path}" and all its contents?`)) return;

    try {
        const response = await fetch(`/api/rmdir?path=${encodeURIComponent(path)}`, {
            method: 'DELETE'
        });

        if (response.ok) {
            loadFiles();
        } else {
            alert('Failed to delete folder');
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Rename file or folder
async function renameItem(oldPath, isDir) {
    const oldName = oldPath.split('/').pop();
    const newName = prompt(`Rename "${oldName}" to:`, oldName);
    if (!newName || newName === oldName) return;

    const validationError = validateFAT32Filename(newName);
    if (validationError) {
        alert(validationError);
        return;
    }

    const pathParts = oldPath.split('/');
    pathParts.pop();
    const newPath = pathParts.length > 1 ? `${pathParts.join('/')}/${newName}` : `/${newName}`;

    try {
        const response = await fetch('/api/rename', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ oldPath, newPath })
        });

        if (response.ok) {
            loadFiles();
        } else {
            alert('Failed to rename');
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Update bulk actions visibility
function updateBulkActions() {
    const checkboxes = document.querySelectorAll('.file-checkbox:checked');
    const selectionInfo = document.getElementById('selectionInfo');
    const downloadBtn = document.getElementById('downloadBtn');
    const renameBtn = document.getElementById('renameBtn');
    const deleteBtn = document.getElementById('deleteBtn');
    const clearBtn = document.getElementById('clearBtn');

    if (checkboxes.length > 0) {
        selectionInfo.style.display = 'inline';
        selectionInfo.textContent = `${checkboxes.length} selected`;
        downloadBtn.style.display = 'inline-block';
        renameBtn.style.display = 'inline-block';
        deleteBtn.style.display = 'inline-block';
        clearBtn.style.display = 'inline-block';
    } else {
        selectionInfo.style.display = 'none';
        downloadBtn.style.display = 'none';
        renameBtn.style.display = 'none';
        deleteBtn.style.display = 'none';
        clearBtn.style.display = 'none';
    }
}

// Clear selection
function clearSelection() {
    document.querySelectorAll('.file-checkbox').forEach(cb => cb.checked = false);
    updateBulkActions();
}

// Download selected (only works for single file)
function downloadSelected() {
    const checkboxes = document.querySelectorAll('.file-checkbox:checked');
    if (checkboxes.length !== 1) {
        alert('Please select exactly one file to download');
        return;
    }
    const isDir = checkboxes[0].dataset.isdir === 'true';
    if (isDir) {
        alert('Cannot download folders');
        return;
    }
    downloadFile(checkboxes[0].value);
}

// Rename selected (only works for single item)
function renameSelected() {
    const checkboxes = document.querySelectorAll('.file-checkbox:checked');
    if (checkboxes.length !== 1) {
        alert('Please select exactly one item to rename');
        return;
    }
    const isDir = checkboxes[0].dataset.isdir === 'true';
    renameItem(checkboxes[0].value, isDir);
}

// Delete selected files
async function deleteSelected() {
    const checkboxes = document.querySelectorAll('.file-checkbox:checked');
    const paths = Array.from(checkboxes).map(cb => cb.value);

    if (paths.length === 0) return;

    if (!confirm(`Delete ${paths.length} selected item(s)?`)) return;

    try {
        for (const path of paths) {
            await fetch(`/api/delete?path=${encodeURIComponent(path)}`, {
                method: 'DELETE'
            });
        }
        loadFiles();
    } catch (error) {
        alert('Error deleting files: ' + error.message);
    }
}

// Load device info on page load
async function loadDeviceSerial() {
    try {
        const response = await fetch('/api/info');
        const data = await response.json();
        const serialElement = document.getElementById('deviceSerial');
        if (serialElement) {
            const heapKB = (data.free_heap / 1024).toFixed(0);
            serialElement.innerHTML = `
                <b>Serial:</b> ${data.serial}<br>
                <b>Firmware:</b> ${data.fw_version}<br>
                <b>Free Heap:</b> ${heapKB} KB
            `;
        }
    } catch (error) {
        console.error('Failed to load device info:', error);
        const serialElement = document.getElementById('deviceSerial');
        if (serialElement) {
            serialElement.textContent = 'Error loading device info';
        }
    }
}

// Load files and device info on page load
loadFiles();
loadDeviceSerial();
