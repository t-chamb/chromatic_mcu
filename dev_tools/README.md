# Chromatic Development Tools

All tools should be run from the `dev_tools/` directory.

```bash
cd dev_tools
```

## Web UI Development

### Chromatic API Server

Test the WiFi file server web UI locally without the device.

```bash
# From dev_tools/
node chromatic-api-server.js
# or
npm run dev
```

Then open http://localhost:8080 in your browser.

## LVGL UI Development

### Option 1: LVGL PC Simulator

Run your LVGL UI natively on your Mac using SDL2.

**Status:** ✅ Working - Shows LVGL demos
**Real OSD Integration:** ⚠️ In Progress - Build system configured, needs header/dependency resolution

#### Setup (one-time)

```bash
# From dev_tools/
./setup_lvgl_simulator.sh
```

#### Run

```bash
# From dev_tools/
./run_lvgl_simulator.sh
# or
npm run lvgl
```

Currently shows LVGL demo widgets. To integrate real OSD code, see `lvgl_simulator/INTEGRATION_GUIDE.md`.

The simulator is ready for:

- UI prototyping and design
- Testing LVGL layouts
- Visual iteration

For full device code integration, additional ESP-IDF stubs and dependency resolution needed.

### Option 2: LVGL Web Simulator

Use Emscripten to compile LVGL to WebAssembly and run in browser.

```bash
# From dev_tools/
./build_web_simulator.sh
python3 -m http.server 8081
```

Open http://localhost:8081

### Option 3: Device Screenshot Tool

Capture screenshots from the actual device over WiFi/serial.

```bash
# From dev_tools/
python3 screenshot_tool.py
```

## Available Scripts

- `chromatic-api-server.js` - Web UI development server with mock APIs
- `setup_lvgl_simulator.sh` - Setup LVGL PC simulator (run once)
- `run_lvgl_simulator.sh` - Build and launch LVGL simulator
- `build_web_simulator.sh` - Build LVGL for web
- `screenshot_tool.py` - Capture device screenshots

## Tips

- Always run tools from `dev_tools/` directory
- Use the simulator for rapid UI iteration
- Test on actual hardware for performance validation
- All dev tools stay in `dev_tools/` and won't be embedded in firmware
