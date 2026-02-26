set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

build_dir := "build"
build_type := "RelWithDebInfo"


default:
    just --list

configure:
    cmake -S . -B {{build_dir}} -DCMAKE_BUILD_TYPE={{build_type}}

build:
    cmake -S . -B {{build_dir}} -DCMAKE_BUILD_TYPE={{build_type}}
    cmake --build {{build_dir}} -j "$(nproc)"

quick:
    cmake --build {{build_dir}} -j "$(nproc)"

run: build
    {{build_dir}}/src/app/kdecodexbar

start: quick
    {{build_dir}}/src/app/kdecodexbar

clean:
    cmake --build {{build_dir}} --target clean

rebuild:
    rm -rf {{build_dir}}
    just build

alias b := build
alias r := run
alias s := start
