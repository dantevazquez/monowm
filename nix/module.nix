{
  config,
  lib,
  pkgs,
  monowmPackage,
}:

let
  cfg = config.services.xserver.windowManager.monowm;
  configuredDefaultPackage = monowmPackage.override {
    inherit (cfg) withBar;
  };
in
{
  options.services.xserver.windowManager.monowm = {
    enable = lib.mkEnableOption "Monowm";

    package = lib.mkOption {
      type = lib.types.package;
      default = configuredDefaultPackage;
      defaultText = lib.literalExpression ''
        monowm.packages.''${pkgs.system}.default.override {
          inherit (config.services.xserver.windowManager.monowm) withBar;
        }
      '';
      description = "The Monowm package to use. A custom package bypasses the withBar option.";
    };

    withBar = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Whether to compile Monowm with its built-in bar.";
    };

    extraSessionCommands = lib.mkOption {
      type = lib.types.lines;
      default = "";
      description = "Shell commands to run immediately before starting Monowm.";
    };

    recommendedPackages = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Install the programs used by Monowm's default configuration.";
    };

    extraPackages = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [ ];
      example = lib.literalExpression "[ pkgs.firefox ]";
      description = "Additional packages to make available in the Monowm session.";
    };
  };

  config = lib.mkIf cfg.enable {
    services.xserver.windowManager.session = lib.singleton {
      name = "monowm";
      start = ''
        ${cfg.extraSessionCommands}
        ${lib.getExe' cfg.package "monowm-start"} &
        waitPID=$!
      '';
    };

    environment.systemPackages = [
      cfg.package
    ]
    ++ lib.optionals cfg.recommendedPackages (
      with pkgs;
      [
        brightnessctl
        dbus
        rofi
        dunst
        xwallpaper
        xcompmgr
        alacritty
        libnotify
        nerd-fonts.jetbrains-mono
        pipewire
        procps
        xrandr
        xset
      ]
    )
    ++ cfg.extraPackages;
  };
}
