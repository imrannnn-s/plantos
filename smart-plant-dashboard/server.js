/* ==========================================================================
   SMART PLANT WATERING DASHBOARD - LOCAL NODE.JS SERVER
   ========================================================================== */

const http = require('http');
const fs = require('fs');
const path = require('path');

const PORT = 3000;

// Dictionary of basic MIME types
const MIME_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.css': 'text/css; charset=utf-8',
    '.js': 'application/javascript; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.png': 'image/png',
    '.jpg': 'image/jpeg',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon'
};

const server = http.createServer((req, res) => {
    console.log(`[Request] ${req.method} ${req.url}`);
    
    // Resolve URL path
    let filePath = req.url === '/' ? '/index.html' : req.url;
    
    // Remove query strings if any
    const queryIndex = filePath.indexOf('?');
    if (queryIndex !== -1) {
        filePath = filePath.substring(0, queryIndex);
    }
    
    // Construct local absolute path
    const absolutePath = path.join(__dirname, filePath);
    
    // Check if the requested file is within the project directory (security check)
    if (!absolutePath.startsWith(__dirname)) {
        res.writeHead(403, { 'Content-Type': 'text/plain; charset=utf-8' });
        res.end('403 Forbidden: Access Denied');
        return;
    }
    
    // Verify file existence
    fs.stat(absolutePath, (err, stats) => {
        if (err || !stats.isFile()) {
            console.error(`[404] File not found: ${absolutePath}`);
            res.writeHead(404, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end('<h1>404 Not Found</h1><p>The requested file does not exist on this server.</p>');
            return;
        }
        
        // Read and serve file
        fs.readFile(absolutePath, (readErr, content) => {
            if (readErr) {
                console.error(`[500] Error reading file: ${readErr.message}`);
                res.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
                res.end('500 Internal Server Error');
                return;
            }
            
            // Deduce Content-Type based on extension
            const ext = path.extname(absolutePath).toLowerCase();
            const contentType = MIME_TYPES[ext] || 'application/octet-stream';
            
            res.writeHead(200, {
                'Content-Type': contentType,
                'Cache-Control': 'no-cache, no-store, must-revalidate',
                'Content-Length': content.length
            });
            res.end(content);
        });
    });
});

server.listen(PORT, () => {
    console.log(`========================================================`);
    console.log(`  Smart Plant Watering System Dashboard Local Server     `);
    console.log(`  Running on: http://localhost:${PORT}                   `);
    console.log(`  Press Ctrl+C to stop the server                      `);
    console.log(`========================================================`);
});
