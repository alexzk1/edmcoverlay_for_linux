#!/usr/bin/env bash
set -e

#First find the folder, where this script resides.
SOURCE=${BASH_SOURCE[0]}
while [ -L "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

#We expect subfolder cpp there.
cd $DIR/cpp


WITH_CAIRO=OFF


if command -v pkg-config >/dev/null 2>&1; then
  if pkg-config --exists cairo; then
    echo "[INFO] Cairo detected via pkg-config."
    WITH_CAIRO=ON
  else
    echo "[INFO] Cairo not found. You can install it with:"
    echo "       Debian/Ubuntu: sudo apt install libcairo2-dev"
    echo "       Fedora:         sudo dnf install cairo-devel"
    echo "       Arch:           sudo pacman -S cairo"
    echo "       Alpine:         sudo apk add cairo-dev"
    echo "       Continuing without Cairo..."
  fi
else
  echo "[WARNING] 'pkg-config' not found in the system."
  echo "          It's required to detect Cairo automatically."
  echo "          To install pkg-config:"
  echo "            Debian/Ubuntu: sudo apt install pkg-config"
  echo "            Fedora:         sudo dnf install pkgconf-pkg-config"
  echo "            Arch:           sudo pacman -S pkgconf"
  echo "            Alpine:         sudo apk add pkgconf"
  echo "          Continuing without Cairo..."
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DWITH_CAIRO=${WITH_CAIRO}
cmake --build build  --verbose

echo ""
echo ""
echo "Everything is prepared. You can start EDMC now."

