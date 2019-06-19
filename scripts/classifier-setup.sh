#!/bin/bash

#
# Step 1: install nvidia driver
#
sudo apt-get update
sudo add-apt-repository ppa:graphics-drivers/ppa
sudo apt-get update
sudo apt-get install nvidia-410
# keep these handy if needed in production
sudo apt-get -d install nvidia-384 
sudo apt-get -d install nvidia-340

# Remove shit that gets installed with nvidia drivers
sudo systemctl stop lightdm.service 
sudo systemctl disable lightdm.service 
sudo apt purge xserver-xorg -y && sudo apt autoremove -y && sudo apt autoclean
sudo apt purge gnome* -y && sudo apt autoremove -y && sudo apt autoclean

# Helpers
sudo apt-get install vim curl pciutils net-tools tcpdump vlan rsyslog snmpd nfs-common

# 
# Step 2: install docker engine
#
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo add-apt-repository "deb [arch=amd64] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable"
sudo apt-get update
apt-cache policy docker-ce
sudo apt-get install -y docker-ce
# sudo systemctl status docker
# sudo usermod -aG docker ${USER}
# Need to log out and log back into the shell for the command above to take effect
# docker info

# 
# Step 3: install nvidia runtime 
#
curl -s -L https://nvidia.github.io/nvidia-container-runtime/gpgkey | sudo apt-key add -
distribution=$(. /etc/os-release;echo $ID$VERSION_ID)
curl -s -L https://nvidia.github.io/nvidia-container-runtime/$distribution/nvidia-container-runtime.list | \
  sudo tee /etc/apt/sources.list.d/nvidia-container-runtime.list
sudo apt-get update
sudo apt-get install nvidia-container-runtime
# sudo mkdir -p /etc/systemd/system/docker.service.d
#sudo tee /etc/systemd/system/docker.service.d/override.conf <<EOF
#[Service]
#ExecStart=
#ExecStart=/usr/bin/dockerd -H unix:// --add-runtime=nvidia=/usr/bin/nvidia-container-runtime
#EOF
#sudo systemctl daemon-reload
#sudo systemctl restart docker 

