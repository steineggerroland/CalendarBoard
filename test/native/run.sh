#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
cjson_dir=${CJSON_DIR:-.pio/libdeps/ota/Arduino_JSON/src}
build_dir=$(mktemp -d /private/tmp/calendarboard-tests.XXXXXX)
cc -fsanitize=address,undefined -g -I"$cjson_dir" -c "$cjson_dir/cjson/cJSON.c" -o "$build_dir/cJSON.o"
c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Ilib/Board -I"$cjson_dir" \
    lib/Board/Board.cpp lib/Board/Protocol.cpp test/native/board_test.cpp "$build_dir/cJSON.o" -o "$build_dir/board-tests"
"$build_dir/board-tests"
