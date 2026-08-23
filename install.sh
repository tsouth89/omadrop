#!/bin/bash
set -euo pipefail

root=$(dirname "$(readlink -f "$0")")
install_root=${OMADROP_INSTALL_ROOT:-${XDG_DATA_HOME:-$HOME/.local/share}/omadrop}
bin_dir=${OMADROP_BIN_DIR:-$HOME/.local/bin}
config_home=${XDG_CONFIG_HOME:-$HOME/.config}
install_dependencies=1
install_bindings=1

usage() {
  cat <<'EOF'
Usage: ./install.sh [--no-deps] [--no-bindings]

Build and install Omadrop for the current user.

  --no-deps      Do not install missing Arch packages
  --no-bindings  Do not add Omarchy keyboard shortcuts
EOF
}

while (($#)); do
  case $1 in
    --no-deps) install_dependencies=0 ;;
    --no-bindings) install_bindings=0 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "install: unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

[[ $install_root == /* && $bin_dir == /* && $config_home == /* ]] || {
  echo "install: XDG and installation paths must be absolute" >&2
  exit 1
}

dependencies=(
  gcc pkgconf libprojectm projectm sdl2-compat glew libpng fftw
  pipewire-audio libpulse imagemagick curl glib2 jq
)
if ((install_dependencies)); then
  if command -v omarchy >/dev/null; then
    omarchy pkg add "${dependencies[@]}"
  elif command -v pacman >/dev/null; then
    sudo pacman -S --needed -- "${dependencies[@]}"
  else
    echo "install: Arch package manager not found; install dependencies from README.md" >&2
    exit 1
  fi
fi

"$root/experiments/projectm-ascii/build.sh"

install -Dm755 "$root/bin/omadrop" "$install_root/bin/omadrop"
install -Dm755 "$root/bin/omadrop-doctor" "$install_root/bin/omadrop-doctor"
install -Dm755 "$root/bin/mpris-art" "$install_root/bin/mpris-art"
install -Dm755 "$root/bin/art-fetch" "$install_root/bin/art-fetch"
install -Dm755 "$root/bin/art-prep" "$install_root/bin/art-prep"
install -Dm755 "$root/experiments/projectm-ascii/projectm-ascii-live" \
  "$install_root/experiments/projectm-ascii/projectm-ascii-live"
install -Dm755 "$root/experiments/projectm-ascii/run-curated.sh" \
  "$install_root/experiments/projectm-ascii/run-curated.sh"
install -Dm755 "$root/uninstall.sh" "$install_root/uninstall.sh"
install -Dm644 "$root/README.md" "$install_root/README.md"
install -Dm644 "$root/CHANGELOG.md" "$install_root/CHANGELOG.md"
install -Dm644 "$root/LICENSE" "$install_root/LICENSE"
install -Dm644 "$root/VERSION" "$install_root/VERSION"
mkdir -p "$bin_dir"
ln -sfn "$install_root/bin/omadrop" "$bin_dir/omadrop"
ln -sfn "$install_root/bin/omadrop-doctor" "$bin_dir/omadrop-doctor"

install_omarchy_bindings() {
  local bindings=$config_home/hypr/bindings.lua
  local keybindings shift_binding alt_binding backup temporary escaped_command
  keybindings=$(omarchy menu keybindings --print 2>/dev/null || true)
  shift_binding=$(grep -E '^SUPER SHIFT \+ V[[:space:]]' <<<"$keybindings" | head -n 1 || true)
  alt_binding=$(grep -E '^SUPER ALT \+ V[[:space:]]' <<<"$keybindings" | head -n 1 || true)
  if [[ -n $shift_binding && $shift_binding != *'→ Omadrop'*
        || -n $alt_binding && $alt_binding != *'→ Toggle Omadrop secondary display'* ]]; then
    echo "install: Omadrop installed, but shortcuts were skipped because one is already in use:" >&2
    [[ -n $shift_binding ]] && echo "  $shift_binding" >&2
    [[ -n $alt_binding ]] && echo "  $alt_binding" >&2
    return
  fi

  mkdir -p "$(dirname "$bindings")"
  touch "$bindings"
  backup=$bindings.bak.$(date +%s)
  cp -p "$bindings" "$backup"
  temporary=$(mktemp "$(dirname "$bindings")/.omadrop-bindings.XXXXXX")
  awk '
    /^-- omadrop:bindings:start$/ { managed = 1; next }
    /^-- omadrop:bindings:end$/ { managed = 0; next }
    managed { next }
    /o\.bind\("SUPER \+ SHIFT \+ V", "Omadrop",/ { next }
    /o\.bind\("SUPER \+ ALT \+ V", "Toggle Omadrop secondary display",/ { next }
    { print }
  ' "$bindings" > "$temporary"
  escaped_command=${bin_dir//\\/\\\\}
  escaped_command=${escaped_command//\"/\\\"}
  cat >> "$temporary" <<EOF

-- omadrop:bindings:start
hl.unbind("SUPER + SHIFT + V")
hl.unbind("SUPER + ALT + V")
o.bind("SUPER + SHIFT + V", "Omadrop", "$escaped_command/omadrop")
o.bind("SUPER + ALT + V", "Toggle Omadrop secondary display", "$escaped_command/omadrop --toggle-secondary")
-- omadrop:bindings:end
EOF
  chmod --reference="$bindings" "$temporary"
  mv "$temporary" "$bindings"

  if command -v hyprctl >/dev/null && [[ ${OMADROP_SKIP_HYPR_RELOAD:-0} != 1 ]]; then
    hyprctl reload >/dev/null
    local errors
    errors=$(hyprctl configerrors 2>&1 || true)
    if [[ -n $errors ]]; then
      cp -p "$backup" "$bindings"
      hyprctl reload >/dev/null || true
      echo "install: Hyprland rejected the shortcuts; restored $backup" >&2
      echo "$errors" >&2
      exit 1
    fi
  fi
}

if ((install_bindings)); then
  if command -v omarchy >/dev/null; then
    install_omarchy_bindings
  else
    echo "install: Omarchy not found; shortcuts were not installed" >&2
  fi
fi

version=$(<"$root/VERSION")
echo "Omadrop $version installed in $install_root"
echo "Run: $bin_dir/omadrop"
if [[ :$PATH: != *":$bin_dir:"* ]]; then
  echo "Add $bin_dir to PATH to run omadrop by name."
fi
