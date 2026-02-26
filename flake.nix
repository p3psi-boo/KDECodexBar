{
  description = "KDECodexBar - AI Usage Tracker for KDE Plasma";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [
          just
          cmake
          extra-cmake-modules
          qt6.qtbase
          qt6.qtdeclarative
          qt6.qtsvg
          qt6.qtwayland
          qt6.qttools
          kdePackages.kstatusnotifieritem
          kdePackages.kcoreaddons
          kdePackages.kconfig
          kdePackages.ki18n
          kdePackages.kwindowsystem
          pkg-config
        ];

        shellHook =
          let
            kdePkgs = with pkgs.kdePackages; [
              kstatusnotifieritem
              kcoreaddons
              kconfig
              ki18n
              kwindowsystem
            ];
            cmakePaths = builtins.concatStringsSep ";" (map (p: "${p}/lib/cmake") kdePkgs);
          in
          ''
            export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.dev.qtPluginPrefix}"
            export CMAKE_PREFIX_PATH="${cmakePaths}:$CMAKE_PREFIX_PATH"
          '';
      };
    };
}
