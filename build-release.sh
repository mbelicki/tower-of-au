#!/bin/bash
# usage: ./build-release.sh project-name /path/to/project/srouce/root/dir

project_name=$1
start_dir=`pwd`
source_dir=`cd "$2"; pwd`
release_dir="$source_dir/../$project_name-release"

echo "Building release version of: '$project_name'..."
if [ -d "$release_dir" ]; then
    echo "Found old release directory, removing..."
    rm -rf $release_dir
fi

mkdir -p "$release_dir"
cd "$release_dir"

cmake -DCMAKE_BUILD_TYPE=Rel "$source_dir" || { echo "Failed to run cmake."; exit; } 
make || { echo "Failed to run make."; exit; }
ctest --output-on-failure . || { echo "Failed to run ctest."; exit; }

cd "$start_dir"
