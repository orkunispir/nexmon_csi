#!/bin/sh

set -Eeuo pipefail
shopt -s inherit_errexit

error() {
    local line_no="$1"
    local func_name="$2"
    local code="${3:-1}"

    # https://stackoverflow.com/questions/5947742/how-to-change-the-output-color-of-echo-in-linux
    local C_FGRED='\033[1;31m'
    local C_NC='\033[0m' # No Color

    echo -e "An ${C_FGRED}ERROR${C_NC} occurred in function ${C_FGRED}${func_name}${C_NC} on or near line ${C_FGRED}$line_no${C_NC}: ${C_FGRED}$BASH_COMMAND${C_NC}"
    echo -e "The program will exit with exit-code ${C_FGRED}${code}${C_NC}\n"

    read -p "Please review the error and press any key to exit this program " -n 1 -r
    exit "${code}"
}

trap 'error $LINENO ${FUNCNAME-main}' ERR

function setStatus () {
  if { [ "$TERM" = "screen" ] && [ -n "$TMUX" ]; } then
    tmux set status-right "Status: $1"   
  fi

  echo "********** ----- Status: $1 ----- ***********"
  echo "$1" >> 'nexmoncsi_status.log'
}

case "$(uname -r)" in
    "5.4."*)
        echo "Running on kernel 5.4.y"
    ;;

    *)
        echo "This script is designed to be run on kernel version 5.4.51, but you seem to be running version $(uname -r)."
        echo "Please use Raspbian Buster Lite 2020-08-20 as indicated in the ReadMe to install Nexmon_CSI."
        read -p "Please review the error and press any key to exit this program " -n 1 -r
        exit 1
    ;;
esac

setStatus "Removing wpasupplicant"
apt remove wpasupplicant -y
echo "denyinterfaces wlan0" >> /etc/dhcpcd.conf

setStatus "Downloading Nexmon"
git clone https://github.com/seemoo-lab/nexmon.git
cd nexmon
NEXDIR=$(pwd)

setStatus "Building libISL"
cd $NEXDIR/buildtools/isl-0.10
autoreconf -f -i
./configure
make
make install
ln -s /usr/local/lib/libisl.so /usr/lib/arm-linux-gnueabihf/libisl.so.10

setStatus "Building libMPFR"
cd $NEXDIR/buildtools/mpfr-3.1.4
autoreconf -f -i
./configure
make
make install
ln -s /usr/local/lib/libmpfr.so /usr/lib/arm-linux-gnueabihf/libmpfr.so.4

setStatus "Setting up Build Environment"
cd $NEXDIR
source setup_env.sh
make

setStatus "Downloading Nexmon_CSI"
cd $NEXDIR/patches/bcm43455c0/7_45_189/
git clone https://github.com/zeroby0/nexmon_csi.git

setStatus "Building and installing Nexmon_CSI"
cd nexmon_csi
git checkout pi-5.4.51
make install-firmware

setStatus "Installing makecsiparams"
cd utils/makecsiparams
make
ln -s $PWD/makecsiparams /usr/local/bin/mcp

setStatus "Installing nexutil"
cd $NEXDIR/utilities/nexutil
make
make install

setStatus "Setting up Persistance"
cd $NEXDIR/patches/bcm43455c0/7_45_189/nexmon_csi/
cd brcmfmac_5.4.y-nexmon
mv $(modinfo brcmfmac -n) ./brcmfmac.ko.orig
cp ./brcmfmac.ko $(modinfo brcmfmac -n)
depmod -a

setStatus "Completed"
