#!/bin/bash

# Setup LVGL PC Simulator for Chromatic UI development
# Run from dev_tools/ directory

set -e

# Ensure we're in dev_tools
if [ ! -f "chromatic-api-server.js" ]; then
    echo "Error: Must run from dev_tools/ directory"
    echo "Usage: cd dev_tools && ./setup_lvgl_simulator.sh"
    exit 1
fi

echo "Setting up LVGL PC Simulator..."

# Check for SDL2
if ! brew list sdl2 &>/dev/null; then
    echo "Installing SDL2..."
    brew install sdl2
fi

# Clone simulator if not exists
if [ ! -d "lvgl_simulator" ]; then
    echo "Cloning LVGL PC simulator..."
    git clone https://github.com/lvgl/lv_port_pc_eclipse.git lvgl_simulator
fi

cd lvgl_simulator

# Initialize LVGL submodule or symlink to project LVGL
if [ -d "lvgl" ] && [ ! "$(ls -A lvgl)" ]; then
    echo "Setting up LVGL library..."
    # Remove empty lvgl directory
    rmdir lvgl
    # Symlink to project's LVGL
    ln -sf ../../managed_components/lvgl__lvgl lvgl
    echo "Using project's LVGL from managed_components"
fi

# Create symlinks to project components
echo "Creating symlinks to OSD components..."
mkdir -p chromatic_components
ln -sf ../../components/osd chromatic_components/osd 2>/dev/null || true
ln -sf ../../components/images chromatic_components/images 2>/dev/null || true
ln -sf ../../components/fonts chromatic_components/fonts 2>/dev/null || true

# Copy lv_conf.h if exists
if [ -f "../../lv_conf.h" ]; then
    echo "Using project lv_conf.h..."
    cp ../../lv_conf.h .
fi

# Create build directory
mkdir -p build

cd ..

# Create launch script
echo "Creating launch script..."
cat > run_lvgl_simulator.sh << 'EOF'
#!/bin/bash

# Launch LVGL Simulator
# Run from dev_tools/ directory

set -e

# Ensure we're in dev_tools
if [ ! -f "chromatic-api-server.js" ]; then
    echo "Error: Must run from dev_tools/ directory"
    exit 1
fi

# Check if simulator is set up
if [ ! -d "lvgl_simulator" ]; then
    echo "Error: LVGL simulator not set up"
    echo "Run: ./setup_lvgl_simulator.sh"
    exit 1
fi

cd lvgl_simulator/build

# Build if needed
if [ ! -f "Makefile" ]; then
    echo "Configuring build..."
    cmake ..
fi

echo "Building simulator..."
make

echo ""
echo "Launching LVGL simulator..."
./lvgl_simulator
EOF

chmod +x run_lvgl_simulator.sh

echo ""
echo "✓ Setup complete!"
echo ""
echo "Created scripts:"
echo "  - run_lvgl_simulator.sh - Build and launch simulator"
echo ""
echo "Next steps (from dev_tools/):"
echo "1. Create your main.c in lvgl_simulator/ with UI initialization"
echo "2. Run: ./run_lvgl_simulator.sh"
