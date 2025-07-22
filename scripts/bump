#!/usr/bin/env bash
# This script bumps the version in misc/Info.plist

MAX_MINISCULE=10
MAX_MINOR=10
MAX_MAJOR=10

# Save plistbuddy path
PLISTBUDDY=$(which /usr/libexec/PlistBuddy)
if [ -z "$PLISTBUDDY" ]; then
    echo "PlistBuddy not found. Please install it."
    exit 1
fi

# Get the current version
CURRENT_VERSION=$($PLISTBUDDY -c "Print :CFBundleVersion" misc/Info.plist)

if [ -z "$CURRENT_VERSION" ]; then
    echo "Current version not found in misc/Info.plist"
    exit 1
fi

# Split the version into components
IFS='.' read -r MAJOR MINOR MINISCULE <<< "$CURRENT_VERSION"

# Increment the miniscule version
MINISCULE=$((MINISCULE + 1))
if [ "$MINISCULE" -ge "$MAX_MINISCULE" ]; then
    MINISCULE=0
    MINOR=$((MINOR + 1))
    if [ "$MINOR" -ge "$MAX_MINOR" ]; then
        MINOR=0
        MAJOR=$((MAJOR + 1))
        if [ "$MAJOR" -ge "$MAX_MAJOR" ]; then
            echo "Maximum version reached: $MAX_MAJOR.$MAX_MINOR.$MAX_MINISCULE"
            exit 1
        fi
    fi
fi

# Construct the new version string
NEW_VERSION="$MAJOR.$MINOR.$MINISCULE"

# Update the plist file with the new version
$PLISTBUDDY -c "Set :CFBundleVersion $NEW_VERSION" misc/Info.plist

# Confirm the update
echo "Version bumped from $CURRENT_VERSION to $NEW_VERSION"
