#!/bin/bash

source_dir=$1
binary_dir=$2
app_name=$3

echo "Creating bundle: '$app_name.app'..."
bundle_dir="$binary_dir/$app_name.app"
if [ -d "$bundle_dir" ]; then
    echo "Found old bundle, removing..."
    rm -rf $bundle_dir
fi

echo "Creating directory structure..."
mkdir -p "$bundle_dir/Contents/MacOS"
mkdir -p "$bundle_dir/Contents/Resources"

echo "Coping files..."
cp -r "$binary_dir/assets" "$bundle_dir/Contents/Resources"
cp -r "$binary_dir/$app_name" "$bundle_dir/Contents/MacOS"
cp -r "$source_dir/resources/Icon.icns" "$bundle_dir/Contents/Resources"
cp -r "$source_dir/osx-bundle/Info.plist" "$bundle_dir/Contents"

echo "Detecting version..."
version=`git -C "$source_dir" describe --tags --dirty --always`
echo "Dectected version: $version."

echo "Updating Info.plist..."
version_escaped="${version//\//\\/}"
sed -i "" "s/APP_VERSION/${version_escaped}/g" "$bundle_dir/Contents/Info.plist"

echo "Done."
