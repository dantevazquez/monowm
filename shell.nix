{
  pkgs ? import <nixpkgs> { },
}:

pkgs.mkShell {
  nativeBuildInputs = with pkgs; [
    gcc
    gnumake
    pkg-config
    clang-tools
  ];

  buildInputs = with pkgs; [
    libx11
    libxcomposite
    libxrender
    libxcb
    libxft
    libxinerama
    libxrandr
    libxext
    freetype
    fontconfig
  ];
}
