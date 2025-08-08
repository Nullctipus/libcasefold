# libcasefold
enable case folding at libc level

## Usage
`casefold application` casefold cwd and run application

`LD_PRELOAD="$HOME/.local/lib/libcasefold.so" application` casefold cwd and run application

`CF_WD="$HOME/fold" casefold application` casefold `$HOME/fold` and run application

## Building
```bash
#ensure you have gcc make and git
sudo pacman -S --needed gcc make git
#clone the repo
git clone https://github.com/Nullctipus/libcasefold
cd libcasefold
# update submodules (needed for testing)
git submodule sync --recursive
# build 
make all
# install (by default to $HOME/.local) can be changed in a config.mak file
make install
```

## Issues
Issues are welcome for suggestions or bug reports