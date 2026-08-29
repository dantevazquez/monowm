# Monowm 🙉

Lightweight x window manger. This window manager follows the mobile workflow where one app/window always occupies the entire screen.

![](demo.gif)

### Features
* Runs at 7.3 mb of ram with a bar and at 2.9 mb of ram with no bar on my system
* Direct keybinds to windows (mod + 1-9) to instantly switch windows 
* Built in MRU switcher
* Built in optional and configurable bar
* Hot reloadble config file for binds

## How to install

### Dependencies

#### Build Dependencies
Install the required tools and X11 development headers for your distribution. The Xft development package is only required when building with the bar.

**Arch-based:**
```bash
sudo pacman -S base-devel libx11 libxft pkgconf
```

**Debian/Ubuntu-based:**
```bash
sudo apt update && sudo apt install build-essential libx11-dev libxft-dev pkg-config
```

**Fedora-based:**
```bash
sudo dnf groupinstall "Development Tools" && sudo dnf install libX11-devel libXft-devel pkgconf-pkg-config
```

**NixOS:**
You can use the provided [shell.nix](shell.nix) to enter a development shell with all build dependencies:
```bash
nix-shell
```

#### Xorg Server (Required Runtime)
Since `monowm` is an X11 window manager, you will need the Xorg server and `xinit` (for `startx`) installed:

**Arch-based:**
```bash
sudo pacman -S xorg-server xorg-xinit
```

**Debian/Ubuntu-based:**
```bash
sudo apt update && sudo apt install xserver-xorg xinit
```

**Fedora-based:**
```bash
sudo dnf install xorg-x11-server-Xorg xinit
```

**NixOS:**
Enable the X11 windowing system in your `/etc/nixos/configuration.nix`:
```nix
services.xserver.enable = true;
```

#### Optional Runtime Dependencies
These are recommended for the default configuration:
* [alacritty](https://github.com/alacritty/alacritty) (default terminal)
* [dmenu](https://tools.suckless.org/dmenu/) (to launch applications)
* [pipewire](https://pipewire.org/) (for volume control)
* [brightnessctl](https://github.com/Hummer12007/brightnessctl) (to control screen brightness)
* [dunst](https://github.com/dunst-project/dunst) (to see volume changes and low battery notifications)
* [xwallpaper](https://github.com/stoeckmann/xwallpaper) (For Background)
* xcompmgr or any x compositor
* A Nerd Font (used by the built-in application icons)

### Installation Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/dantevazquez/monowm.git
   cd monowm
   ```
2. Build and install:
   ```bash
   make install
   # Or with nix: nix-shell --run "make install"
   ```
3. Run `startx` or launch from your favorite display manager.

### Building without the bar

To remove the bar and its Xft dependency from the compiled binary, add this to the ignored local `config.mk` file:

```make
NOBAR := 1
```

Then run `make`. Build artifacts are kept separately for bar and no-bar builds, so switching the value is safe. For a one-off build, use `make NOBAR=1`.

### Nix flake / NixOS

Build Monowm directly from GitHub:

```bash
nix build github:dantevazquez/monowm
```

To use the included NixOS module, add Monowm to the `inputs` of your system flake:

```nix
inputs.monowm = {
  url = "github:dantevazquez/monowm";
  inputs.nixpkgs.follows = "nixpkgs";
};
```

Then import the module in your `nixosSystem`:

```nix
modules = [
  ./configuration.nix
  inputs.monowm.nixosModules.default
];
```

Enable the X server and Monowm in `configuration.nix`:

```nix
{
  services.xserver.enable = true;
  services.xserver.windowManager.monowm.enable = true;

  # Optional: compile Monowm without its built-in bar or Xft dependency.
  services.xserver.windowManager.monowm.withBar = false;

  # Optional: start Monowm automatically instead of choosing it at login.
  services.displayManager.defaultSession = "none+monowm";

  #Optional: Don't insall with recommend packages like alacritty, dmenu, xcompmgr, etc.
  services.xserver.windowManager.monowm.recommendedPackages = false;
}
```

## Configuration
* Core configurations (bindings and custom hotkeys) can be configured in `~/.config/monowm/config.conf` (see template: [config.conf](templates/config.conf)). You can also find the default binds here.
* The built-in bar is configured in `~/.config/monowm/bar.conf` (see template: [bar.conf](templates/bar.conf)). Use `program_padding` for horizontal space inside open-program entries and `vertical_padding` for space above and below the text. `active_text_color` controls the text inside the focused entry; active highlights always fill the complete bar height.
* Startup commands and display settings can be customized in `~/.config/monowm/autostart` (see default: [autostart](autostart)).
* Set status text without another bar process by changing the root window name.:

```bash
xsetroot -name "$(date)"
```

Changes take effect after `monowm --reload` or with ctrl f4.

## Coming soon...
* Bug fixes

