add_executable(tests EXCLUDE_FROM_ALL
    test_judge.c
    "${PROJECT_SOURCE_DIR}/src/common/judge.c"
)
target_include_directories(tests PRIVATE "${PROJECT_SOURCE_DIR}/src/common")
configure_target(tests)

# Проверки assert должны работать и в Release.
target_compile_options(tests PRIVATE -UNDEBUG)

add_test(NAME judge COMMAND tests)
