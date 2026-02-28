#!/bin/bash

# Configuration
INSTALL_DIR="$HOME/.local/bin"
UATU_DIR="$HOME/.uatu"
ZSHRC="$HOME/.zshrc"

# Detect OS for sed differences
OS_TYPE=$(uname)
if [ "$OS_TYPE" = "Darwin" ]; then
    SED_INPLACE="sed -i ''"
else
    SED_INPLACE="sed -i"
fi

echo "Checking dependencies..."
if ! command -v make >/dev/null; then
    echo "Error: 'make' not found."
    exit 1
fi

echo "Building uatu..."
make clean
make

if [ ! -f "uatu" ]; then
    echo "Build failed. Ensure 'sqlite3' development headers are installed."
    exit 1
fi

echo "Installing binary to $INSTALL_DIR..."
mkdir -p "$INSTALL_DIR"
cp uatu "$INSTALL_DIR/uatu"

echo "Setting up integration script in $UATU_DIR..."
mkdir -p "$UATU_DIR"
cp uaterc.zsh "$UATU_DIR/uaterc.zsh"

# Update path in the script to point to installed binary
# We only want to replace the fallback UATU_BIN value, not all occurrences
$SED_INPLACE 's|UATU_BIN="./uatu"|UATU_BIN="'"$INSTALL_DIR/uatu"'"|' "$UATU_DIR/uaterc.zsh"

echo "Updating shell config ($ZSHRC)..."
# Use ~ notation to be more readable
SOURCE_LINE="source \$HOME/.uatu/uaterc.zsh"

if grep -F "$SOURCE_LINE" "$ZSHRC" > /dev/null 2>&1; then
    echo "Already configured in $ZSHRC"
else
    echo "" >> "$ZSHRC"
    echo "# Uatu: Directory Time Tracker" >> "$ZSHRC"
    echo "$SOURCE_LINE" >> "$ZSHRC"
    echo "Added uatu to $ZSHRC"
fi

# Final check for PATH
if [[ ":$PATH:" != ":$INSTALL_DIR:" ]]; then
    echo ""
    echo "WARNING: $INSTALL_DIR is not in your PATH."
    echo "Add this to your $ZSHRC for convenience:"
    echo "  export PATH=\"
$PATH:$INSTALL_DIR\""
fi

echo "-----------------------------------"
echo "Installation complete!"
echo "Please restart your terminal or run:"
echo "  source ~/.zshrc"
echo "-----------------------------------"