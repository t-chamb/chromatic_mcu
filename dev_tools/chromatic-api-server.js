#!/usr/bin/env node

const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 8080;
const WWW_DIR = path.join(__dirname, '../components/wifi_file_server/www');

// Mock file system structure
const mockFileSystem = {
  '/': [
    { name: 'folder1', path: '/folder1', is_dir: true },
    { name: 'folder2', path: '/folder2', is_dir: true },
    ...Array.from({ length: 50 }, (_, i) => ({
      name: `video${i + 1}.mp4`,
      path: `/video${i + 1}.mp4`,
      is_dir: false,
      size: Math.floor(Math.random() * 5000000) + 100000
    })),
  ],
  '/folder1': [
    { name: 'subfolder', path: '/folder1/subfolder', is_dir: true },
    { name: 'file1.txt', path: '/folder1/file1.txt', is_dir: false, size: 512 },
    { name: 'file2.txt', path: '/folder1/file2.txt', is_dir: false, size: 1024 },
  ],
  '/folder1/subfolder': [
    { name: 'deep.mp4', path: '/folder1/subfolder/deep.mp4', is_dir: false, size: 5120000 },
  ],
  '/folder2': [
    { name: 'another.mp4', path: '/folder2/another.mp4', is_dir: false, size: 3072000 },
  ],
};

const server = http.createServer((req, res) => {
  const timestamp = new Date().toISOString().split('T')[1].split('.')[0];
  console.log(`[${timestamp}] ${req.method} ${req.url}`);
  
  // CORS headers
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, DELETE');
  
  if (req.method === 'OPTIONS') {
    res.writeHead(200);
    res.end();
    return;
  }
  
  // API endpoints
  if (req.url.startsWith('/api/list')) {
    const url = new URL(req.url, `http://localhost:${PORT}`);
    const path = url.searchParams.get('path') || '/';
    const page = parseInt(url.searchParams.get('page') || '1');
    const perPage = 10;
    
    const allFiles = mockFileSystem[path] || [];
    const fileCount = allFiles.filter(f => !f.is_dir).length;
    const totalPages = Math.ceil(fileCount / perPage);
    
    // Separate dirs and files
    const dirs = allFiles.filter(f => f.is_dir);
    const files = allFiles.filter(f => !f.is_dir);
    
    // Paginate files only (dirs always shown)
    const startIdx = (page - 1) * perPage;
    const endIdx = startIdx + perPage;
    const paginatedFiles = files.slice(startIdx, endIdx);
    
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      files: [...dirs, ...paginatedFiles],
      page: page,
      total_pages: totalPages,
      total_files: fileCount,
      ip: '192.168.4.1'
    }));
    return;
  }
  
  if (req.url.startsWith('/api/download')) {
    res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
    res.end('Mock file content');
    return;
  }
  
  if (req.url.startsWith('/api/delete')) {
    const url = new URL(req.url, `http://localhost:${PORT}`);
    const filePath = url.searchParams.get('path');
    
    console.log(`  → Deleted: ${filePath}`);
    
    // Remove from mock filesystem
    for (const dir in mockFileSystem) {
      mockFileSystem[dir] = mockFileSystem[dir].filter(f => f.path !== filePath);
    }
    
    res.writeHead(200);
    res.end('OK');
    return;
  }
  
  // API: Get system info
  if (req.url === '/api/info' && req.method === 'GET') {
    const randomHex = () => Math.floor(Math.random() * 256).toString(16).toUpperCase().padStart(2, '0');
    const fakeSerial = `CHR-${randomHex()}${randomHex()}${randomHex()}${randomHex()}${randomHex()}${randomHex()}`;
    const fakeHeap = Math.floor(Math.random() * 100000) + 50000; // 50-150KB
    
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      serial: fakeSerial,
      fw_version: 'v1.2.3-dev',
      free_heap: fakeHeap,
      min_heap: fakeHeap - 10000,
      ssid: 'Chromatic_MCU'
    }));
    return;
  }
  
  // API: Get settings
  if (req.url === '/api/settings' && req.method === 'GET') {
    const randomHex = () => Math.floor(Math.random() * 256).toString(16).toUpperCase().padStart(2, '0');
    const fakeSerial = `CHR-${randomHex()}${randomHex()}${randomHex()}${randomHex()}${randomHex()}${randomHex()}`;
    
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      ssid: 'Chromatic_MCU',
      password: 'chromatic123',
      hide_ssid: false,
      serial: fakeSerial
    }));
    return;
  }
  
  // API: Save settings
  if (req.url === '/api/settings' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => {
      body += chunk.toString();
    });
    req.on('end', () => {
      try {
        const settings = JSON.parse(body);
        console.log(`  → Settings updated: SSID="${settings.ssid}", Hide SSID=${settings.hide_ssid}`);
        res.writeHead(200);
        res.end('OK');
      } catch (error) {
        res.writeHead(400);
        res.end('Bad Request');
      }
    });
    return;
  }
  
  // API: Create folder
  if (req.url === '/api/mkdir' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => {
      body += chunk.toString();
    });
    req.on('end', () => {
      try {
        const { path } = JSON.parse(body);
        console.log(`  → Created folder: ${path}`);
        
        // Add to mock filesystem
        if (!mockFileSystem[path]) {
          mockFileSystem[path] = [];
        }
        
        // Add folder entry to parent
        const parentPath = path.substring(0, path.lastIndexOf('/')) || '/';
        const folderName = path.split('/').pop();
        if (mockFileSystem[parentPath]) {
          mockFileSystem[parentPath].push({
            name: folderName,
            path: path,
            is_dir: true
          });
        }
        
        res.writeHead(200);
        res.end('OK');
      } catch (error) {
        res.writeHead(400);
        res.end('Bad Request');
      }
    });
    return;
  }
  
  // API: Delete folder
  if (req.url.startsWith('/api/rmdir')) {
    const url = new URL(req.url, `http://localhost:${PORT}`);
    const folderPath = url.searchParams.get('path');
    
    console.log(`  → Deleted folder: ${folderPath}`);
    
    // Remove from mock filesystem
    delete mockFileSystem[folderPath];
    for (const dir in mockFileSystem) {
      mockFileSystem[dir] = mockFileSystem[dir].filter(f => f.path !== folderPath);
    }
    
    res.writeHead(200);
    res.end('OK');
    return;
  }
  
  // API: Rename file/folder
  if (req.url === '/api/rename' && req.method === 'POST') {
    let body = '';
    req.on('data', chunk => {
      body += chunk.toString();
    });
    req.on('end', () => {
      try {
        const { oldPath, newPath } = JSON.parse(body);
        console.log(`  → Renamed: ${oldPath} -> ${newPath}`);
        
        // Update in mock filesystem
        for (const dir in mockFileSystem) {
          mockFileSystem[dir] = mockFileSystem[dir].map(f => {
            if (f.path === oldPath) {
              return { ...f, name: newPath.split('/').pop(), path: newPath };
            }
            return f;
          });
        }
        
        res.writeHead(200);
        res.end('OK');
      } catch (error) {
        res.writeHead(400);
        res.end('Bad Request');
      }
    });
    return;
  }
  
  if (req.url.startsWith('/api/upload')) {
    let body = '';
    req.on('data', chunk => {
      body += chunk.toString();
    });
    req.on('end', () => {
      // Extract filename from multipart form data
      const filenameMatch = body.match(/filename="([^"]+)"/);
      const filename = filenameMatch ? filenameMatch[1] : 'unknown';
      
      console.log(`  → Uploaded: ${filename}`);
      
      // Simulate adding file to mock filesystem (always to root for simplicity)
      const uploadPath = '/';
      if (!mockFileSystem[uploadPath]) {
        mockFileSystem[uploadPath] = [];
      }
      
      // Add to mock filesystem if not already there
      const newFile = {
        name: filename,
        path: `/${filename}`,
        is_dir: false,
        size: Math.floor(Math.random() * 1000000) + 10000
      };
      
      // Check if file already exists
      const existingIndex = mockFileSystem[uploadPath].findIndex(f => f.name === filename);
      if (existingIndex >= 0) {
        mockFileSystem[uploadPath][existingIndex] = newFile;
      } else {
        mockFileSystem[uploadPath].push(newFile);
      }
      
      res.writeHead(200);
      res.end('OK');
    });
    return;
  }
  
  // Serve static files from www directory
  let filePath = path.join(WWW_DIR, req.url.split('?')[0]);
  if (req.url === '/' || req.url === '') {
    filePath = path.join(WWW_DIR, 'index.html');
  }
  
  // Serve index.html for any path that's not a static file
  if (!req.url.match(/\.(html|css|js)$/)) {
    filePath = path.join(WWW_DIR, 'index.html');
  }
  
  const extname = path.extname(filePath);
  const contentTypes = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript',
  };
  
  const contentType = contentTypes[extname] || 'text/plain';
  
  fs.readFile(filePath, (err, content) => {
    if (err) {
      res.writeHead(404);
      res.end('404 Not Found');
    } else {
      res.writeHead(200, { 'Content-Type': contentType });
      res.end(content);
    }
  });
});

server.listen(PORT, () => {
  console.log(`Mock server running at http://localhost:${PORT}/`);
  console.log(`Serving files from: ${WWW_DIR}`);
  console.log('API endpoints available:');
  console.log('  GET  /api/list');
  console.log('  GET  /api/info');
  console.log('  GET  /api/settings');
  console.log('  POST /api/settings');
  console.log('  GET  /api/download');
  console.log('  POST /api/upload');
  console.log('  POST /api/mkdir');
  console.log('  DELETE /api/delete');
  console.log('  DELETE /api/rmdir');
  console.log('  POST /api/rename');
});
