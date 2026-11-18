#!/usr/bin/env fish
# Запуск HTTP-клиента C++ под Arch Linux. API_URL можно изменить извне.
set -l project_dir (dirname (status filename))
cd -- "$project_dir"; or exit 1

if not test -x ./build/bank_cli
    echo 'Клиент не собран. Выполните: cmake --preset arch-release; and cmake --build --preset arch-release' >&2
    exit 1
end

exec ./build/bank_cli
