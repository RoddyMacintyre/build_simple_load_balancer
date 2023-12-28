#!/bin/bash
sudo apt-get update

sudo apt-get install -y vim build-essential git checkout -b branch_name

sudo apt-get install python-dev
sudo apt-get install python-pip

sudo pip install fabric
sudo pip install pexpect
sudo pip install pyflakes
sudo pip install requests

git clone https://github.com/LuaJIT/LuaJIT/tree/v2.1.ROLLING
cd luajit-2.1
make && sudo make install