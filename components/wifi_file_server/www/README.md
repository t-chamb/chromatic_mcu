# Chromatic WiFi File Server Web UI

## Development Setup

```bash
cd components/wifi_file_server/www
npm install
```

## Development Workflow

### 1. Local Development
Run a local web server to test changes:
```bash
npm run dev
```
Then open http://localhost:8080 in your browser.

### 2. Lint & Validate
Check for errors before building:
```bash
npm run lint
```

This runs:
- `htmlhint` - HTML validation
- `stylelint` - CSS linting
- `eslint` - JavaScript linting

### 3. Build for Firmware
Minify files before embedding:
```bash
npm run build
```

This creates:
- `index.html.min`
- `style.css.min`
- `app.js.min`

### 4. Update CMakeLists.txt
After building, update the CMakeLists.txt to use minified files:
```cmake
target_add_binary_data(${COMPONENT_LIB} "www/index.html.min" BINARY)
target_add_binary_data(${COMPONENT_LIB} "www/style.css.min" BINARY)
target_add_binary_data(${COMPONENT_LIB} "www/app.js.min" BINARY)
```

## Mock API for Local Testing

When running locally, the API calls will fail. You can:
1. Use browser DevTools to mock responses
2. Create a simple Node.js mock server
3. Point to a running device's IP

## File Structure

```
www/
├── index.html          # Main HTML (source)
├── style.css           # Styles (source)
├── app.js              # JavaScript (source)
├── *.min               # Minified versions (generated)
├── package.json        # Node dependencies
├── build.js            # Build script
└── README.md           # This file
```
