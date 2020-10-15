#!/bin/sh

function setStatus () {
  if { [ "$TERM" = "screen" ] && [ -n "$TMUX" ]; } then
    tmux set status-right "Status: $1"
    echo "##################********** Status: $1 **********##################"
  else
    echo "##################********** Status: $1 **********##################"
  fi
}

cd nexmon
NEXDIR=$(pwd)
source setup_env.sh

setStatus "Downloading Nexmon_CSI"
cd $NEXDIR/patches/bcm43455c0/7_45_189/
git clone https://github.com/zeroby0/nexmon_csi.git

setStatus "Building and installing Nexmon_CSI"
cd nexmon_csi
# git checkout 4fea4172a12155f2d2c977e206fdec56d11faf79
git checkout release-pi-buster-4.19.97-plus
make install-firmware

setStatus "Installing makecsiparams"
cd utils/makecsiparams
make
ln -s $PWD/makecsiparams /usr/local/bin/mcp

setStatus "Setting up Persistance"
cd $NEXDIR/patches/bcm43455c0/7_45_189/nexmon_csi/
cd brcmfmac_4.19.y-nexmon
mv $(modinfo brcmfmac -n) ./brcmfmac.ko.orig
cp ./brcmfmac.ko $(modinfo brcmfmac -n)
depmod -a

touch /home/pi/COMPLETED
setStatus "Completed"