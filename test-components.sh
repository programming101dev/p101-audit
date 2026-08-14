#!/usr/bin/env bash
# Additional component suites for the consolidated p101-audit repository.

run_component_tests() {
  local component="$1"
  local component_name="${component##*/}"
  local component_test_bd

  if [ -n "$test_cache_root" ]; then
    component_test_bd="$test_cache_root/components/$component_name/build-$sfx"
    mkdir -p "$(dirname "$component_test_bd")"
  else
    component_test_bd="$component/test/build-$sfx"
  fi

  if [ "$coverage" -eq 1 ]; then
    rm -rf "$component_test_bd"
  fi
  echo ">> configuring $component_name tests ($component_test_bd)"
  cmake -S "$component/test" -B "$component_test_bd" \
    -U 'P101_*_LIBRARY' "$compflag" \
    ${compiler_driver_args[@]+"${compiler_driver_args[@]}"} \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON "$compile_flag_arg" \
    ${sanitizer_args[@]+"${sanitizer_args[@]}"} \
    ${p101_path_args[@]+"${p101_path_args[@]}"} "$cov_arg" >/dev/null
  if [ "$coverage" -eq 1 ]; then
    find "$component_test_bd" -type f -name '*.gcda' -exec rm -f {} +
  fi
  echo ">> building $component_name tests"
  cmake --build "$component_test_bd"
  echo ">> ctest: $component_name"
  ( cd "$component_test_bd" && ctest --output-on-failure ${ctest_args[@]+"${ctest_args[@]}"} )
}

run_component_tests components/error-contract
run_component_tests components/module-map
run_component_tests components/doctor
