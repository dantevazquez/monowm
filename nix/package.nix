{
  stdenv,
  lib,
  makeWrapper,
  pkg-config,
  libX11,
  libxcb,
  libXft,
  libXinerama,
  libXrandr,
  libXext,
  freetype,
  fontconfig,
}:

stdenv.mkDerivation {
  pname = "monowm";
  version = "0-unstable-2026-07-21";

  src = lib.cleanSourceWith {
    src = ../.;
    filter =
      path: type:
      let
        name = baseNameOf (toString path);
        isBuildArtifact =
          type == "regular" && (name == "monowm" || name == "lemonbar" || lib.hasSuffix ".o" name);
      in
      lib.cleanSourceFilter path type && !isBuildArtifact;
  };

  nativeBuildInputs = [
    makeWrapper
    pkg-config
  ];

  buildInputs = [
    libX11
    libxcb
    libXft
    libXinerama
    libXrandr
    libXext
    freetype
    fontconfig
  ];

  strictDeps = true;

  installPhase = ''
    runHook preInstall

    install -Dm755 monowm lemonbar monowm-start monowm-volume \
      monowm-brightness -t "$out/bin"
    install -Dm644 monowm.desktop "$out/share/xsessions/monowm.desktop"
    install -Dm644 templates/config.conf templates/bar.conf autostart bg.png \
      -t "$out/share/monowm"

    patchShebangs "$out/bin" "$out/share/monowm/autostart"
    wrapProgram "$out/bin/monowm-start" \
      --set MONOWM_DEFAULTS_DIR "$out/share/monowm" \
      --prefix PATH : "$out/bin"

    runHook postInstall
  '';

  meta = {
    description = "Lightweight fullscreen X11 window manager";
    homepage = "https://github.com/dantevazquez/monowm";
    mainProgram = "monowm";
    platforms = lib.platforms.linux;
  };
}
