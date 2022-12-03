# xGUI-Pro AUR package packaging script

## xGUI-Pro release version Packaging script

```bash
# The following operations are done under the Arch Linux release version.
# Other Linux distributions can be referenced.
$ cd aur/xguipro

# Modify the version number to the current latest release number
$ vim PKGBUILD
# eg., pkgver=0.8.1
# Save and exit

# Update the check value of the xGUI-Pro release version and generate a .SRCINFO file
$ updpkgsums && makepkg --printsrcinfo > .SRCINFO

# Compile and package the xGUI-Pro release version
$ makepkg -sf

# Install the xGUI-Pro package
$ yay -U xguipro-*.tar.zst

# Compile, package and install
$ makepkg -sfi

# Online installation via AUR: [xguipro](https://aur.archlinux.org/packages/xguipro)
$ yay -S xguipro
```

## xGUI-Pro development version Packaging script

```bash
# The following operations are done under the Arch Linux release version.
# Other Linux distributions can be referenced.
$ cd aur/xguipro-git

# The xGUI-Pro development version does not need to update the check values and .SRCINFO files when compiling.
# Updates are required only when submitted to AUR.
# Update the check value of the xGUI-Pro development version and generate a .SRCINFO file
$ updpkgsums && makepkg --printsrcinfo > .SRCINFO

# Compile and package the xGUI-Pro development version
$ makepkg -sf

# Install the xGUI-Pro package
$ yay -U xguipro-git-*.tar.zst

# Compile, package and install
$ makepkg -sfi

# Online installation via AUR: [xguipro-git](https://aur.archlinux.org/packages/xguipro-git)
$ yay -S xguipro-git
```
