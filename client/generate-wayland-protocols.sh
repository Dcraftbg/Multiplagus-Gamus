#!/bin/sh
set -xe

PROTO_DIR=$(pkg-config --variable=pkgdatadir wayland-protocols)
mkdir -p wayland

# --- Handle xdg-toplevel-icon separately ---
if [ -f "$PROTO_DIR/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml" ]; then
  XDG_TOPLEVEL_ICON_XML="$PROTO_DIR/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml"
elif [ -f "$PROTO_DIR/unstable/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml" ]; then
  XDG_TOPLEVEL_ICON_XML="$PROTO_DIR/unstable/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml"
else
  echo "⚠️  xdg-toplevel-icon-v1.xml missing — fetching from upstream..."
  mkdir -p wayland/protocols/staging/xdg-toplevel-icon
  curl -L -o wayland/protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml \
    https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/main/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml
  XDG_TOPLEVEL_ICON_XML="wayland/protocols/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml"
fi

# --- Stable protocols ---
wayland-scanner client-header \
  "$PROTO_DIR/stable/xdg-shell/xdg-shell.xml" \
  wayland/xdg-shell.h

wayland-scanner private-code \
  "$PROTO_DIR/stable/xdg-shell/xdg-shell.xml" \
  wayland/xdg-shell-protocol.c

# --- Unstable protocols ---
PROTOS="$PROTO_DIR/unstable"

wayland-scanner private-code \
  "$PROTOS/xdg-decoration/xdg-decoration-unstable-v1.xml" \
  wayland/xdg-decoration-unstable-v1-protocol.c

wayland-scanner private-code \
  "$PROTOS/pointer-constraints/pointer-constraints-unstable-v1.xml" \
  wayland/pointer-constraints-unstable-v1-protocol.c

wayland-scanner private-code \
  "$PROTOS/relative-pointer/relative-pointer-unstable-v1.xml" \
  wayland/relative-pointer-unstable-v1-protocol.c

wayland-scanner client-header \
  "$PROTOS/xdg-output/xdg-output-unstable-v1.xml" \
  wayland/xdg-output-unstable-v1.h

wayland-scanner private-code \
  "$PROTOS/xdg-output/xdg-output-unstable-v1.xml" \
  wayland/xdg-output-unstable-v1-protocol.c

wayland-scanner client-header \
  "$PROTOS/xdg-decoration/xdg-decoration-unstable-v1.xml" \
  wayland/xdg-decoration-unstable-v1.h

wayland-scanner client-header \
  "$PROTOS/relative-pointer/relative-pointer-unstable-v1.xml" \
  wayland/relative-pointer-unstable-v1.h

wayland-scanner client-header \
  "$PROTOS/pointer-constraints/pointer-constraints-unstable-v1.xml" \
  wayland/pointer-constraints-unstable-v1.h

wayland-scanner client-header \
  "$PROTOS/xdg-output/xdg-output-unstable-v1.xml" \
  wayland/xdg-output-unstable-v1.h

# --- Finally the missing one ---
wayland-scanner client-header \
  "$XDG_TOPLEVEL_ICON_XML" \
  wayland/xdg-toplevel-icon-v1.h

wayland-scanner private-code \
  "$XDG_TOPLEVEL_ICON_XML" \
  wayland/xdg-toplevel-icon-v1-protocol.c

echo "✅ All Wayland protocol headers and sources generated successfully."
