#!/usr/bin/env bash

# Change script permissions
chmod u+x ./scripts/*
chmod u+x build.sh

# Symlink `./scripts` directory to `./s`
if [ ! -d "./s" ]; then
    ln -s ./scripts ./s
    echo "Symlink created: ./s -> ./scripts"
else
    echo "Symlink already exists: ./s"
fi
