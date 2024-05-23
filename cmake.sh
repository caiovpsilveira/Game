# run from build
export CC="$(which clang)"
export CXX="$(which clang++)"

cmake .. -DCMAKE_USER_MAKE_RULES_OVERRIDE="CMakeOverrides.txt" "$@"
