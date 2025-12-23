{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # Build our minimal parser (unwrapped)
        minimal-parser-unwrapped = pkgs.clangStdenv.mkDerivation {
          pname = "minimal-cling-parser";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.cmake ];
          buildInputs = with pkgs; [
            cling.unwrapped
            llvmPackages_18.llvm
          ];

          cmakeBuildType = "Release";

          meta = with pkgs.lib; {
            description = "Minimal C++ parser using Cling interpreter";
            license = licenses.bsd3;
            platforms = platforms.unix;
            mainProgram = "minimal-parser";
          };
        };

        # Wrapped version with proper cling flags
        minimal-parser = minimal-parser-unwrapped.overrideAttrs (oldAttrs: {
          nativeBuildInputs = oldAttrs.nativeBuildInputs ++ [ pkgs.makeWrapper ];

          # minimal-parser needs a collection of flags to start up properly, so wrap it by default.
          # We'll provide the unwrapped version as a passthru
          flags = pkgs.cling.flags ++ [
            "-resource-dir"
            "${pkgs.cling.unwrapped}"
            "-L"
            "${pkgs.cling.unwrapped}/lib"
            "-l"
            "${pkgs.cling.unwrapped}/lib/cling.so"
          ];

          fixupPhase = ''
            runHook preFixup

            wrapProgram $out/bin/minimal-parser \
              --argv0 $out/bin/.minimal-parser-wrapped \
              --add-flags "$flags"

            runHook postFixup
          '';

          passthru = (oldAttrs.passthru or { }) // {
            unwrapped = minimal-parser-unwrapped;
          };
        });

      in
      {
        packages = {
          default = minimal-parser;
          minimal-parser = minimal-parser;
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            python3
          ];

          shellHook = ''
            echo "Development shell for minimal Cling parser"
            echo "Build with: nix build"
            echo "Run with: ./result/bin/minimal-parser"
          '';
        };

        apps.default = flake-utils.lib.mkApp {
          drv = minimal-parser;
          name = "minimal-parser";
        };
      });
}
