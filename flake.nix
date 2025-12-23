{
  description = "Minimal C++ parser using Cling interpreter";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        # Nixpkgs moved to argparse 3.x, but we need ~2.9 (from xeus-cling.nix)
        argparse_2_9 = pkgs.argparse.overrideAttrs (oldAttrs: {
          version = "2.9";
          src = pkgs.fetchFromGitHub {
            owner = "p-ranav";
            repo = "argparse";
            rev = "v2.9";
            sha256 = "sha256-vbf4kePi5gfg9ub4aP1cCK1jtiA65bUS9+5Ghgvxt/E=";
          };
        });

        # Nixpkgs moved to xeus 5.2.0, but we need 3.2.0 (from xeus-cling.nix)
        xeus_3_2_0 = pkgs.xeus.overrideAttrs (oldAttrs: {
          version = "3.2.0";
          src = pkgs.fetchFromGitHub {
            owner = "jupyter-xeus";
            repo = "xeus";
            tag = "3.2.0";
            sha256 = "sha256-D/dJ0SHxTHJw63gHD6FRZS7O2TVZ0voIv2mQASEjLA8=";
          };
          buildInputs = oldAttrs.buildInputs ++ pkgs.lib.singleton pkgs.xtl;
        });

        # Build our minimal parser (unwrapped)
        minimal-parser-unwrapped = pkgs.clangStdenv.mkDerivation {
          pname = "minimal-cling-parser";
          version = "0.1.0";

          src = ./.;

          nativeBuildInputs = [ pkgs.cmake ];
          buildInputs = with pkgs; [
            cling.unwrapped
            cppzmq
            libuuid
            llvmPackages_18.llvm
            ncurses
            openssl
            pugixml
            xeus-zmq
            xtl
            zeromq
            zlib
          ] ++ [
            argparse_2_9
            xeus_3_2_0
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
            cmake
            clang
            cling.unwrapped
            cppzmq
            libuuid
            llvmPackages_18.llvm
            ncurses
            openssl
            pugixml
            xeus-zmq
            xtl
            zeromq
            zlib
          ] ++ [
            argparse_2_9
            xeus_3_2_0
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
