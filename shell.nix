{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gcc
    gnumake
    pkg-config
    clang-tools
  ];

  buildInputs = with pkgs; [
    libx11
  ];
}
