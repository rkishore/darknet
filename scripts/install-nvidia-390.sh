#!/bin/bash
sudo apt-get update
sudo add-apt-repository ppa:graphics-drivers/ppa
sudo apt-get update
sudo apt-get install nvidia-390

# Optional if other shit gets installed with nvidia drivers
sudo systemctl stop lightdm.service 
sudo systemctl disable lightdm.service 
sudo apt purge xserver-xorg -y && sudo apt autoremove -y && sudo apt autoclean
sudo apt purge gnome* -y && sudo apt autoremove -y && sudo apt autoclean
