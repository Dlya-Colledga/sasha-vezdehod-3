#!/usr/bin/env fish
# Запуск сервера C++ под Arch Linux. Рабочая папка всегда корень проекта.
set -l project_dir (dirname (status filename))
cd -- "$project_dir"; or exit 1

if not test -x ./build/bank_server
    echo 'Сервер не собран. Выполните: cmake --preset arch-release; and cmake --build --preset arch-release' >&2
    exit 1
end

exec ./build/bank_server
