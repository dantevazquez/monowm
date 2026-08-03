{
  config,
  lib,
  pkgs,
  monowmPackage,
}:

let
  cfg = config.services.xserver.windowManager.monowm;
in
{
  options.services.xserver.windowManager.monowm = {
    enable = lib.mkEnableOption "Monowm";

    package = lib.mkOption {
      type = lib.types.package;
      default = monowmPackage;
      defaultText = lib.literalExpression "monowm.packages.\${pkgs.system}.default";
      description = "The Monowm package to use.";
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
        dmenu
        dunst
        feh
        kitty
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
