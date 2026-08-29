{
  stdenv,
  lib,
  makeWrapper,
  pkg-config,
  libX11,
  libXft,
  withBar ? true,
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
        isBuildArtifact = type == "regular" && (name == "monowm" || lib.hasSuffix ".o" name);
      in
      lib.cleanSourceFilter path type && !isBuildArtifact;
  };

  nativeBuildInputs = [
    makeWrapper
    pkg-config
  ];

  buildInputs = [ libX11 ] ++ lib.optionals withBar [ libXft ];

  strictDeps = true;
  dontConfigure = true;

  makeFlags = [ "NOBAR=${if withBar then "0" else "1"}" ];

  installPhase = ''
    runHook preInstall

    install -Dm755 monowm monowm-start monowm-volume monowm-brightness \
      -t "$out/bin"
    install -Dm644 monowm.desktop "$out/share/xsessions/monowm.desktop"
    install -Dm644 templates/config.conf autostart bg.png \
      -t "$out/share/monowm"
    ${lib.optionalString withBar ''
      install -Dm644 templates/bar.conf -t "$out/share/monowm"
    ''}

    # strictDeps keeps Bash out of HOST_PATH, so the default --host lookup
    # leaves /bin/bash untouched.  Patch with the build-time interpreter
    # before wrapProgram moves monowm-start to .monowm-start-wrapped.
    patchShebangs --build "$out/bin"
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
