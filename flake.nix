{
  description = "Monowm - a lightweight fullscreen X11 window manager";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      supportedSystems = [
        "aarch64-linux"
        "x86_64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          monowm = pkgs.callPackage ./nix/package.nix { };
          default = self.packages.${system}.monowm;
        }
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.monowm ];
            packages = [ pkgs.clang-tools ];
          };
        }
      );

      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-tree);

      nixosModules = {
        monowm =
          {
            config,
            lib,
            pkgs,
            ...
          }:
          import ./nix/module.nix {
            inherit config lib pkgs;
            monowmPackage = self.packages.${pkgs.stdenv.hostPlatform.system}.monowm;
          };
        default = self.nixosModules.monowm;
      };
    };
}
