{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs; [
    gcc
    pkg-config
    glew
    wayland
    wayland-protocols
    wayland-scanner
    libxkbcommon
    curl  # for optional fetching in the script
  ];
}
